#ifndef CONFIG_H
#define CONFIG_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <ctime>

struct Particle {
  float x, y;
  float vx, vy;
  int color;
  int _pad;
};

struct Config {
  int num_particles;
  int num_colors;
  float r_max;
  float canvas_size_factor;
  float dt;
  float damping;
  float speed_limit;

  // matrix_a[c1 * num_colors + c2], matrix_b[c1 * num_colors + c2]
  std::vector<float> matrix_a;
  std::vector<float> matrix_b;

  static constexpr int max_colors = 32;
  static const float bright_colors[max_colors][3];

  bool parse_line(const std::string& line) {
    if (line.empty() || line[0] == '#' || line[0] == ';') return true;

    auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) return true;

    std::string key = trim(line.substr(0, eq_pos));
    std::string value = trim(line.substr(eq_pos + 1));

    try {
      if (key == "num_particles") {
        num_particles = std::stoi(value);
      } else if (key == "num_colors") {
        num_colors = std::stoi(value);
      } else if (key == "r_max") {
        r_max = std::stof(value);
      } else if (key == "canvas_size_factor") {
        canvas_size_factor = std::stof(value);
      } else if (key == "dt") {
        dt = std::stof(value);
      } else if (key == "damping") {
        damping = std::stof(value);
      } else if (key == "speed_limit") {
        speed_limit = std::stof(value);
      } else if (key == "matrix_a") {
        matrix_a = parse_floats(value);
      } else if (key == "matrix_b") {
        matrix_b = parse_floats(value);
      }
    } catch (const std::exception& e) {
      std::cerr << "Warning: Failed to parse '" << key << "': " << e.what() << std::endl;
    }
    return true;
  }

  bool load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
      parse_line(line);
    }

    int expected = num_colors * num_colors;
    if ((int)matrix_a.size() != expected || (int)matrix_b.size() != expected) {
      randomize_matrix();
      return true;
    }
    return true;
  }

  void randomize_matrix() {
    int n = num_colors * num_colors;
    matrix_a.resize(n);
    matrix_b.resize(n);
    std::srand(std::time(nullptr));
    for (int i = 0; i < n; i++) {
      matrix_a[i] = 2.0f * ((float)std::rand() / RAND_MAX) - 1.0f;
      matrix_b[i] = 2.0f * ((float)std::rand() / RAND_MAX) - 1.0f;
    }
  }

  void save(const std::string& path) {
    std::ofstream file(path);
    file << "# Particle Life Configuration\n";
    file << "# Generated on: " << __DATE__ << " " << __TIME__ << "\n";
    file << "num_particles = " << num_particles << "\n";
    file << "num_colors = " << num_colors << "\n";
    file << "r_max = " << r_max << "\n";
    file << "canvas_size_factor = " << canvas_size_factor << "\n";
    file << "dt = " << dt << "\n";
    file << "damping = " << damping << "\n";
    file << "speed_limit = " << speed_limit << "\n";

    file << "matrix_a = ";
    for (size_t i = 0; i < matrix_a.size(); i++) {
      if (i > 0) file << ", ";
      file << matrix_a[i];
    }
    file << "\n";

    file << "matrix_b = ";
    for (size_t i = 0; i < matrix_b.size(); i++) {
      if (i > 0) file << ", ";
      file << matrix_b[i];
    }
    file << "\n";
  }

  float canvas_size() const { return canvas_size_factor * r_max; }
  int grid_cells() const { return (int)canvas_size_factor; }

private:
  static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
  }

  static std::vector<float> parse_floats(const std::string& s) {
    std::vector<float> result;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
      token = trim(token);
      if (!token.empty()) {
        result.push_back(std::stof(token));
      }
    }
    return result;
  }
};

constexpr const float Config::bright_colors[Config::max_colors][3] = {
  {1.0f, 0.2f, 0.2f}, {0.2f, 1.0f, 0.2f}, {0.2f, 0.2f, 1.0f},
  {1.0f, 1.0f, 0.2f}, {1.0f, 0.2f, 1.0f}, {0.2f, 1.0f, 1.0f},
  {1.0f, 0.6f, 0.2f}, {0.6f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.6f},
  {1.0f, 0.4f, 0.6f}, {0.6f, 0.8f, 1.0f}, {1.0f, 0.8f, 0.4f},
  {0.8f, 0.4f, 1.0f}, {0.4f, 1.0f, 0.8f}, {1.0f, 0.6f, 0.6f},
  {0.6f, 1.0f, 0.6f}, {1.0f, 0.8f, 0.8f}, {0.8f, 1.0f, 0.8f},
  {0.8f, 0.8f, 1.0f}, {1.0f, 1.0f, 0.8f}, {0.2f, 0.6f, 1.0f},
  {1.0f, 0.2f, 0.6f}, {0.6f, 1.0f, 0.2f}, {1.0f, 0.6f, 0.2f},
  {0.2f, 0.8f, 0.8f}, {0.8f, 0.2f, 0.8f}, {1.0f, 0.9f, 0.2f},
  {0.2f, 1.0f, 0.9f}, {0.9f, 0.2f, 1.0f}, {0.9f, 0.9f, 0.2f},
  {0.2f, 0.9f, 0.9f}, {0.9f, 0.2f, 0.9f},
};

#endif
