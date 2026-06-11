[![en](https://img.shields.io/badge/lang-en-green)](README.md)
[![zh-cn](https://img.shields.io/badge/lang-%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-green)](README.zh-cn.md)

# PARTLIFE

GPU-based 粒子生命模拟

---

[效果](assets/50000-32colors-20factor.gif)

图为 50000 粒子模拟, 在我本地 RTX 5070 下能跑 60 FPS.

## 构建与运行

```bash
make -j$(nproc)
```

以构建.

```bash
make run
```

以运行.

## 配置文件

根目录的 `config.cfg` 是默认配置文件. 在 `matrix_*` 未设置的情况下, 会随机生成. 每次程序运行, 会保存一份配置到 `config_saved.cfg`, 确保可复现.

一些配置项解释:

- canvas_size_factor: 值越大, "细胞" 相对尺寸越小.
- dt: 值越大, 系统演化速度越快.
- damping: 值越大, 粒子减速越慢.

## 依赖

| 依赖 | 版本 (已测试) |
| - | - |
| CUDA Toolkit (nvcc) | ≥ 13.1 |
| C++ 编译器 (g++) | ≥ 12.3 (C++20) |
| GLFW3 | ≥ 3.3 |
| GLEW | ≥ 2.2 |
| OpenGL | ≥ 3.3 (Core Profile) |

在 Ubuntu/Debian 上安装依赖:

```bash
sudo apt install build-essential libglfw3-dev libglew-dev
```

CUDA Toolkit 请从 [NVIDIA 官网](https://developer.nvidia.com/cuda-downloads) 下载安装.
