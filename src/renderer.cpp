#include "renderer.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cuda_runtime.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <sstream>
#include <cmath>
#include <algorithm>

static const char* vertex_shader_source = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;

out vec3 vColor;

uniform vec2 uScale;
uniform float uPointSize;

void main() {
  gl_Position = vec4(aPos.x * uScale.x - 1.0, aPos.y * uScale.y - 1.0, 0.0, 1.0);
  gl_PointSize = uPointSize;
  vColor = aColor;
}
)";

static const char* fragment_shader_source = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
  vec2 coord = gl_PointCoord - vec2(0.5);
  float dist = length(coord);
  if (dist > 0.5) discard;

  FragColor = vec4(vColor, 1.0);
}
)";

Renderer::Renderer(const Config& cfg, int w, int h)
  : config(cfg)
  , window_width(w)
  , window_height(h)
  , window(nullptr)
  , vao(0)
  , vbo(0)
  , shader_program(0)
  , fps(0.0)
  , frame_count(0)
  , last_time(0.0)
{
}

Renderer::~Renderer() {
  if (vbo) glDeleteBuffers(1, &vbo);
  if (vao) glDeleteVertexArrays(1, &vao);
  if (shader_program) glDeleteProgram(shader_program);
  if (window) glfwDestroyWindow(window);
  glfwTerminate();
}

bool Renderer::init() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window = glfwCreateWindow(window_width, window_height, "Particle Life", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    return false;
  }

  if (!setup_shaders()) {
    return false;
  }

  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  size_t vertex_size = sizeof(float) * 5;
  glBufferData(GL_ARRAY_BUFFER, config.num_particles * vertex_size, nullptr, GL_DYNAMIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex_size, (void*)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)(sizeof(float) * 2));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  float point_size = 1000.0f / std::sqrt((float)config.num_particles);
  glUseProgram(shader_program);
  glUniform1f(glGetUniformLocation(shader_program, "uPointSize"), point_size);

  float canvas_size = config.canvas_size();
  float scale_x = 2.0f / canvas_size;
  float scale_y = 2.0f / canvas_size;
  float aspect = (float)window_width / (float)window_height;
  if (aspect > 1.0f) {
    scale_x /= aspect;
  } else {
    scale_y *= aspect;
  }
  glUniform2f(glGetUniformLocation(shader_program, "uScale"), scale_x, scale_y);

  last_time = glfwGetTime();

  std::cout << "Renderer initialized: " << window_width << "x" << window_height
        << ", point size " << point_size << std::endl;
  return true;
}

bool Renderer::setup_shaders() {
  unsigned int vs = compile_shader(vertex_shader_source, GL_VERTEX_SHADER);
  if (!vs) return false;

  unsigned int fs = compile_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
  if (!fs) return false;

  shader_program = link_program(vs, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);

  return shader_program != 0;
}

unsigned int Renderer::compile_shader(const std::string& source, unsigned int type) {
  unsigned int shader = glCreateShader(type);
  const char* src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(shader, 512, nullptr, info_log);
    std::cerr << "Shader compilation error (" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
          << "): " << info_log << std::endl;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

unsigned int Renderer::link_program(unsigned int vs, unsigned int fs) {
  unsigned int program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(program, 512, nullptr, info_log);
    std::cerr << "Program linking error: " << info_log << std::endl;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

void Renderer::render(const Particle* d_particles, int num_particles) {
  std::vector<float> host_data(num_particles * 5);

  std::vector<Particle> host_particles(num_particles);
  cudaMemcpy(host_particles.data(), d_particles, num_particles * sizeof(Particle), cudaMemcpyDeviceToHost);

  for (int i = 0; i < num_particles; i++) {
    host_data[i * 5 + 0] = host_particles[i].x;
    host_data[i * 5 + 1] = host_particles[i].y;

    int color_idx = host_particles[i].color;
    if (color_idx >= Config::max_colors) color_idx = color_idx % Config::max_colors;
    host_data[i * 5 + 2] = Config::bright_colors[color_idx][0];
    host_data[i * 5 + 3] = Config::bright_colors[color_idx][1];
    host_data[i * 5 + 4] = Config::bright_colors[color_idx][2];
  }

  glClear(GL_COLOR_BUFFER_BIT);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_particles * 5 * sizeof(float), host_data.data());

  glBindVertexArray(vao);
  glDrawArrays(GL_POINTS, 0, num_particles);
  glBindVertexArray(0);

  glfwSwapBuffers(window);

  // FPS 计算
  frame_count++;
  double current_time = glfwGetTime();
  if (current_time - last_time >= 1.0) {
    fps = frame_count / (current_time - last_time);
    frame_count = 0;
    last_time = current_time;

    std::string title = "Particle Life - FPS: " + std::to_string((int)fps)
              + " - Particles: " + std::to_string(num_particles);
    glfwSetWindowTitle(window, title.c_str());
  }
}

void Renderer::check_gl_error(const char* location) {
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    const char* err_str = "unknown";
    switch (err) {
      case GL_INVALID_ENUM: err_str = "GL_INVALID_ENUM"; break;
      case GL_INVALID_VALUE: err_str = "GL_INVALID_VALUE"; break;
      case GL_INVALID_OPERATION: err_str = "GL_INVALID_OPERATION"; break;
      case GL_OUT_OF_MEMORY: err_str = "GL_OUT_OF_MEMORY"; break;
      case GL_INVALID_FRAMEBUFFER_OPERATION: err_str = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
    }
    std::cerr << "OpenGL error at " << location << ": " << err_str << " (0x" << std::hex << err << std::dec << ")" << std::endl;
  }
}

bool Renderer::should_close() const {
  return window ? glfwWindowShouldClose(window) : true;
}

void Renderer::poll_events() {
  glfwPollEvents();
}
