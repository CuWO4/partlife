#ifndef SIMULATION_CUH
#define SIMULATION_CUH

#include "config.h"

class Simulation {
public:
  Simulation(const Config& config);
  ~Simulation();

  void init_particles();
  void step();

  const Particle* get_particles() const { return d_particles; }
  int get_num_particles() const { return config.num_particles; }
  float get_canvas_size() const { return config.canvas_size(); }

private:
  Config config;

  Particle* d_particles;
  int* d_cell_indices;
  int* d_cell_starts;
  int* d_cell_ends;
  float* d_matrix_a;
  float* d_matrix_b;
  float* d_matrix_c;
  float* d_matrix_d;
  Particle* d_sorted_particles;

  int num_cells;
  int grid_dim;
  int frame_count;

  static constexpr int BLOCK_SIZE = 256;
};

#endif
