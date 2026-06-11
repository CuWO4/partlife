#include "config.h"
#include "simulation.cuh"
#include "renderer.h"
#include <cuda_runtime.h>
#include <iostream>
#include <string>
#include <filesystem>

int main(int argc, char* argv[]) {
  std::cout << "=== Particle Life Simulation ===" << std::endl;
  std::cout << std::endl;

  std::string config_path = "config.cfg";
  if (argc >= 2) config_path = argv[1];

  Config config;
  bool config_loaded = false;
  if (std::filesystem::exists(config_path)) {
    std::cout << "Loading " << config_path << std::endl;
    config_loaded = config.load(config_path);
  }

  if (!config_loaded) {
    config.randomize_matrix();
  }

  // Hardcoded rules for color 0
  // matrix[ci][0]: color 0 does not affect other colors (except itself)
  // matrix[0][0]: color 0 strongly repels itself
  // matrix[0][cj>0]: other colors slightly repel color 0
  {
    int n = config.num_colors;
    auto set = [&](int ci, int cj, float a, float b, float c, float d) {
      int idx = ci * n + cj;
      config.matrix_a[idx] = a;
      config.matrix_b[idx] = b;
      config.matrix_c[idx] = c;
      config.matrix_d[idx] = d;
    };
    for (int ci = 0; ci < n; ci++) {
      for (int cj = 0; cj < n; cj++) {
        if (ci == 0 && cj == 0) {
          set(0, 0, -1.0f, -1.0f, -1.0f, 0.0f);           // self-repulsion
        } else if (cj == 0) {
          set(ci, 0, 0.0f, 0.0f, 0.0f, 0.0f);           // no effect on others
        } else if (ci == 0) {
          set(0, cj, -0.2f, -0.05f, -0.02f, 0.0f);          // slightly repelled by others
        }
      }
    }
  }

  std::string save_path = "config_saved.cfg";
  config.save(save_path);
  std::cout << "Saved config to " << save_path << std::endl;
  std::cout << std::endl;

  std::cout << "Configuration:" << std::endl;
  std::cout << "  Particles:  " << config.num_particles << std::endl;
  std::cout << "  Colors:     " << config.num_colors << std::endl;
  std::cout << "  r_max:    " << config.r_max << std::endl;
  std::cout << "  Canvas Factor:" << config.canvas_size_factor << std::endl;
  std::cout << "  dt:       " << config.dt << std::endl;
  std::cout << "  Damping:    " << config.damping << std::endl;
  std::cout << "  Speed Limit:  " << config.speed_limit << std::endl;
  std::cout << "  Canvas:     " << config.canvas_size() << " x " << config.canvas_size() << std::endl;
  std::cout << "  Grid:     " << config.grid_cells() << " x " << config.grid_cells() << std::endl;
  std::cout << std::endl;

  Simulation simulation(config);
  int window_size = 1024;
  if (config.num_particles > 20000) {
    window_size = 1280;
  }
  if (config.num_particles > 40000) {
    window_size = 1440;
  }

  Renderer renderer(config, window_size, window_size);
  if (!renderer.init()) {
    return 1;
  }

  cudaDeviceSynchronize();

  std::cout << "=== Running ===" << std::endl;
  std::cout << std::endl;

  while (!renderer.should_close()) {
    simulation.step();
    renderer.render(simulation.get_particles(), simulation.get_num_particles());
    renderer.poll_events();
  }

  std::cout << "Simulation ended. Configuration was saved to: " << save_path << std::endl;

  return 0;
}
