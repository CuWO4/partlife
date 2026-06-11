#ifndef RENDERER_H
#define RENDERER_H

#include "config.h"

struct GLFWwindow;

class Renderer {
public:
  Renderer(const Config& config, int window_width = 1024, int window_height = 1024);
  ~Renderer();

  bool init();
  void render(const Particle* d_particles, int num_particles);
  bool should_close() const;
  double get_fps() const { return fps; }
  void poll_events();

private:
  const Config& config;
  int window_width;
  int window_height;

  GLFWwindow* window;

  unsigned int vao;
  unsigned int vbo;
  unsigned int shader_program;

  double fps;
  int frame_count;
  double last_time;

  unsigned int compile_shader(const std::string& source, unsigned int type);
  unsigned int link_program(unsigned int vertex_shader, unsigned int fragment_shader);
  bool setup_shaders();
  void check_gl_error(const char* location);
};

#endif
