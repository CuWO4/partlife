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
