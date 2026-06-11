[![en](https://img.shields.io/badge/lang-en-green)](README.md)
[![zh-cn](https://img.shields.io/badge/lang-%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-green)](README.zh-cn.md)

# PARTLIFE

GPU-accelerated particle life simulation

---

[Demo](assets/50000-32colors-20factor.gif)

Simulation of 50,000 particles running at 60 FPS on an RTX 5070.

## Build & Run

```bash
make -j$(nproc)
```

Build the project.

```bash
make run
```

Run the simulation. Optionally specify a config file:

```bash
./debug/release_build-*/main my_config.cfg
```

## Configuration

The default config file is `config.cfg` at the project root. If `matrix_*` fields are not provided, random values are generated. On each run, the configuration (including the full matrix) is saved to `config_saved.cfg` to ensure reproducibility.

Key parameters:

- **canvas_size_factor**: Larger value → smaller "cells" relative to the canvas.
- **dt**: Larger value → faster evolution.
- **damping**: Larger value → less velocity damping (particles slow down slower).

## Dependencies

| Dependency | Tested Version |
| - | - |
| CUDA Toolkit (nvcc) | ≥ 13.1 |
| C++ compiler (g++) | ≥ 12.3 (C++20) |
| GLFW3 | ≥ 3.3 |
| GLEW | ≥ 2.2 |
| OpenGL | ≥ 3.3 (Core Profile) |

On Ubuntu/Debian:

```bash
sudo apt install build-essential libglfw3-dev libglew-dev
```

Download CUDA Toolkit from [NVIDIA's website](https://developer.nvidia.com/cuda-downloads).
