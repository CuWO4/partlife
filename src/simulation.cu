#include "simulation.cuh"
#include <thrust/device_ptr.h>
#include <thrust/sort.h>
#include <thrust/copy.h>
#include <cuda_runtime.h>
#include <iostream>
#include <cmath>
#include <cstdlib>

__global__ void init_kernel(Particle* particles, int n, float canvas_size, int num_colors, unsigned long long seed) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;

  unsigned long long rng = seed + i * 6364136223846793005ULL;
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  particles[i].x = (float)(rng >> 33) / (float)(1ULL << 31) * canvas_size;
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  particles[i].y = (float)(rng >> 33) / (float)(1ULL << 31) * canvas_size;
  particles[i].vx = 0.0f;
  particles[i].vy = 0.0f;
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  particles[i].color = (int)((float)(rng >> 33) / (float)(1ULL << 31) * num_colors);
  if (particles[i].color >= num_colors) particles[i].color = num_colors - 1;
  if (particles[i].color < 0) particles[i].color = 0;
  particles[i]._pad = 0;
}

__global__ void compute_cell_indices_kernel(const Particle* particles, int* cell_indices,
                        int n, float r_max, int grid_dim) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;

  int cx = (int)(particles[i].x / r_max);
  int cy = (int)(particles[i].y / r_max);
  if (cx < 0) cx = 0;
  if (cx >= grid_dim) cx = grid_dim - 1;
  if (cy < 0) cy = 0;
  if (cy >= grid_dim) cy = grid_dim - 1;
  cell_indices[i] = cy * grid_dim + cx;
}

__global__ void build_cell_ranges_kernel(const int* cell_indices, int* cell_starts,
                       int* cell_ends, int n, int num_cells) {
  if (threadIdx.x == 0 && blockIdx.x == 0) {
    for (int c = 0; c < num_cells; c++) {
      cell_starts[c] = n;
      cell_ends[c] = n;
    }
    if (n == 0) return;

    // 第一个粒子
    int prev_cell = cell_indices[0];
    cell_starts[prev_cell] = 0;

    for (int j = 1; j < n; j++) {
      int cur_cell = cell_indices[j];
      if (cur_cell != prev_cell) {
        cell_ends[prev_cell] = j;
        cell_starts[cur_cell] = j;
        prev_cell = cur_cell;
      }
    }
    cell_ends[prev_cell] = n;
  }
}

__global__ void compute_acceleration_kernel(
  const Particle* particles,
  const int* cell_starts,
  const int* cell_ends,
  const float* matrix_a,
  const float* matrix_b,
  float* ax,
  float* ay,
  int n,
  float r_max,
  float canvas_size,
  int grid_dim,
  int num_colors)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;

  float px = particles[i].x;
  float py = particles[i].y;
  int ci = particles[i].color;

  int cx = (int)(px / r_max);
  int cy = (int)(py / r_max);
  if (cx < 0) cx = 0;
  if (cx >= grid_dim) cx = grid_dim - 1;
  if (cy < 0) cy = 0;
  if (cy >= grid_dim) cy = grid_dim - 1;

  float total_ax = 0.0f;
  float total_ay = 0.0f;

  float half_canvas = canvas_size * 0.5f;

  #define ORIGIN_REJECTION -3.0f
  // Piecewise: (0, ORIGIN_REJECTION), (r_max/10, 0), (r_max/3, a), (2*r_max/3, b), (r_max, 0)
  float r_rep = r_max * 0.1f;
  float r_third = r_max / 3.0f;
  float r_two_third = 2.0f * r_max / 3.0f;
  float inv_r_rep = 1.0f / r_rep;
  float inv_r_rep_to_a = 1.0f / (r_third - r_rep);
  float inv_r_a_to_b = 1.0f / (r_two_third - r_third);
  float inv_r_b_to_0 = 1.0f / (r_max - r_two_third);

  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      int nx = (cx + dx + grid_dim) % grid_dim;
      int ny = (cy + dy + grid_dim) % grid_dim;

      int cell_idx = ny * grid_dim + nx;
      int start = cell_starts[cell_idx];
      int end = cell_ends[cell_idx];

      for (int j = start; j < end; j++) {
        if (j == i) continue;

        float dx_val = particles[j].x - px;
        float dy_val = particles[j].y - py;

        if (dx_val > half_canvas) dx_val -= canvas_size;
        else if (dx_val < -half_canvas) dx_val += canvas_size;
        if (dy_val > half_canvas) dy_val -= canvas_size;
        else if (dy_val < -half_canvas) dy_val += canvas_size;

        float dist_sq = dx_val * dx_val + dy_val * dy_val;

        if (dist_sq > 0.0f && dist_sq < r_max * r_max) {
          float dist = sqrtf(dist_sq);
          int cj = particles[j].color;

          float a_val = matrix_a[ci * num_colors + cj];
          float b_val = matrix_b[ci * num_colors + cj];

          float accel;
          if (dist < r_rep) {
            accel = ORIGIN_REJECTION + dist * inv_r_rep;
          } else if (dist < r_third) {
            float t = (dist - r_rep) * inv_r_rep_to_a;
            accel = ORIGIN_REJECTION + (a_val - ORIGIN_REJECTION) * t;
          } else if (dist < r_two_third) {
            float t = (dist - r_third) * inv_r_a_to_b;
            accel = a_val + (b_val - a_val) * t;
          } else {
            float t = (dist - r_two_third) * inv_r_b_to_0;
            accel = b_val * (1.0f - t);
          }

          float inv_dist = 1.0f / dist;
          total_ax += accel * dx_val * inv_dist;
          total_ay += accel * dy_val * inv_dist;
        }
      }
    }
  }

  ax[i] = total_ax;
  ay[i] = total_ay;
}

__global__ void drift_matrix_kernel(float* matrix_a, float* matrix_b, int size) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= size) return;

  unsigned long long rng = (unsigned long long)(i * 2654435761ULL + clock64());
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  float drift_a = ((rng >> 33) & 1) ? 0.01f : -0.01f;
  matrix_a[i] += drift_a;
  if (matrix_a[i] > 1.0f) matrix_a[i] = 1.0f;
  if (matrix_a[i] < -1.0f) matrix_a[i] = -1.0f;

  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  float drift_b = ((rng >> 33) & 1) ? 0.01f : -0.01f;
  matrix_b[i] += drift_b;
  if (matrix_b[i] > 1.0f) matrix_b[i] = 1.0f;
  if (matrix_b[i] < -1.0f) matrix_b[i] = -1.0f;
}

__global__ void update_kernel(
  Particle* particles,
  const float* ax,
  const float* ay,
  int n,
  float dt,
  float damping,
  float speed_limit,
  float canvas_size)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;

  particles[i].vx += ax[i] * dt;
  particles[i].vy += ay[i] * dt;

  particles[i].vx *= damping;
  particles[i].vy *= damping;

  float speed_sq = particles[i].vx * particles[i].vx + particles[i].vy * particles[i].vy;
  if (speed_sq > speed_limit * speed_limit) {
    float inv_speed = speed_limit / sqrtf(speed_sq);
    particles[i].vx *= inv_speed;
    particles[i].vy *= inv_speed;
  }

  particles[i].x += particles[i].vx * dt;
  particles[i].y += particles[i].vy * dt;

  if (particles[i].x < 0.0f) particles[i].x += canvas_size;
  if (particles[i].x >= canvas_size) particles[i].x -= canvas_size;
  if (particles[i].y < 0.0f) particles[i].y += canvas_size;
  if (particles[i].y >= canvas_size) particles[i].y -= canvas_size;
}

Simulation::Simulation(const Config& cfg)
  : config(cfg), frame_count(0)
{
  grid_dim = config.grid_cells();
  num_cells = grid_dim * grid_dim;

  int n = config.num_particles;
  cudaMalloc(&d_particles, n * sizeof(Particle));
  cudaMalloc(&d_cell_indices, n * sizeof(int));
  cudaMalloc(&d_cell_starts, num_cells * sizeof(int));
  cudaMalloc(&d_cell_ends, num_cells * sizeof(int));
  cudaMalloc(&d_sorted_particles, n * sizeof(Particle));

  int matrix_size = config.num_colors * config.num_colors;
  cudaMalloc(&d_matrix_a, matrix_size * sizeof(float));
  cudaMalloc(&d_matrix_b, matrix_size * sizeof(float));

  cudaMemcpy(d_matrix_a, config.matrix_a.data(), matrix_size * sizeof(float), cudaMemcpyHostToDevice);
  cudaMemcpy(d_matrix_b, config.matrix_b.data(), matrix_size * sizeof(float), cudaMemcpyHostToDevice);

  init_particles();

  std::cout << "Simulation initialized: " << n << " particles, "
        << config.num_colors << " colors, grid " << grid_dim << "x" << grid_dim
        << ", canvas " << config.canvas_size() << "x" << config.canvas_size() << std::endl;
}

Simulation::~Simulation() {
  cudaFree(d_particles);
  cudaFree(d_cell_indices);
  cudaFree(d_cell_starts);
  cudaFree(d_cell_ends);
  cudaFree(d_matrix_a);
  cudaFree(d_matrix_b);
  cudaFree(d_sorted_particles);
}

void Simulation::init_particles() {
  int n = config.num_particles;
  int block_size = BLOCK_SIZE;
  int grid_size = (n + block_size - 1) / block_size;

  unsigned long long seed = (unsigned long long)std::time(nullptr);
  init_kernel<<<grid_size, block_size>>>(
    d_particles, n, config.canvas_size(), config.num_colors, seed);
  cudaDeviceSynchronize();
}

void Simulation::step() {
  int n = config.num_particles;
  int block_size = BLOCK_SIZE;
  int grid_size = (n + block_size - 1) / block_size;

  compute_cell_indices_kernel<<<grid_size, block_size>>>(
    d_particles, d_cell_indices, n, config.r_max, grid_dim);

  {
    using namespace thrust;
    device_ptr<Particle> src(d_particles);
    device_ptr<int> keys(d_cell_indices);
    device_ptr<Particle> dst(d_sorted_particles);
    copy(src, src + n, dst);
    sort_by_key(keys, keys + n, dst);
    copy(dst, dst + n, src);
  }

  compute_cell_indices_kernel<<<grid_size, block_size>>>(
    d_particles, d_cell_indices, n, config.r_max, grid_dim);

  build_cell_ranges_kernel<<<1, 1>>>(
    d_cell_indices, d_cell_starts, d_cell_ends, n, num_cells);

  float *d_ax, *d_ay;
  cudaMalloc(&d_ax, n * sizeof(float));
  cudaMalloc(&d_ay, n * sizeof(float));

  compute_acceleration_kernel<<<grid_size, block_size>>>(
    d_particles, d_cell_starts, d_cell_ends,
    d_matrix_a, d_matrix_b,
    d_ax, d_ay, n, config.r_max, config.canvas_size(), grid_dim,
    config.num_colors);

  update_kernel<<<grid_size, block_size>>>(
    d_particles, d_ax, d_ay, n,
    config.dt, config.damping, config.speed_limit,
    config.canvas_size());

  cudaFree(d_ax);
  cudaFree(d_ay);

  cudaDeviceSynchronize();

  frame_count++;
  if (frame_count % 60 == 0) {
    int matrix_size = config.num_colors * config.num_colors;
    int mgrid = (matrix_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    drift_matrix_kernel<<<mgrid, BLOCK_SIZE>>>(d_matrix_a, d_matrix_b, matrix_size);
    cudaDeviceSynchronize();
  }
}
