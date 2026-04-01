# ImgToolAi

一款参考 [Imageshop](https://www.cnblogs.com/Imageshop/) 设计理念，基于 **Dear ImGui + OpenGL + C++17** 构建的轻量级跨平台图像处理桌面工具。

---

## 功能特性

### 基础调整
- **亮度（Brightness）**：对每个像素的 RGB 值加/减偏移量，结果 clamp 到 [0, 255]
- **对比度（Contrast）**：以 128 为中心进行缩放
- **伽马校正（Gamma Correction）**：标准幂次校正

### 颜色调整
- **HSL 调整**：色相 / 饱和度 / 亮度三轴独立调整
- **色彩平衡（Color Balance）**：RGB 三通道增益
- **灰度化（Grayscale）**：使用加权公式 0.299R + 0.587G + 0.114B
- **反色（Invert）**：255 - 每个通道值
- **色调分离（Posterize）**：将通道量化到指定级数

### 滤镜效果
- **高斯模糊（Gaussian Blur）**：可分离两趟卷积，半径 1–20
- **锐化（Sharpen）**：3×3 锐化卷积核
- **浮雕（Emboss）**：3×3 浮雕卷积核
- **边缘检测（Edge Detection）**：Sobel 算子
- **去雾（Dehaze）**：基于暗通道先验的简化实现

### 变换操作
- **缩放（Resize）**：双线性插值
- **旋转**：90° / 180° / 270°
- **翻转**：水平 / 垂直

### 文件支持
- **打开**：PNG、JPG、BMP、TGA（通过 stb_image）
- **保存**：PNG、JPG、BMP、TGA（通过 stb_image_write）

### 交互体验
- 鼠标滚轮缩放图像（以鼠标位置为中心）
- 鼠标左键 / 中键拖拽平移
- 底部状态栏实时显示鼠标所指像素坐标和 RGBA 值
- **撤销 / 重做**（Ctrl+Z / Ctrl+Y，最多 20 步）

---

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++17 |
| GUI | [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) |
| 窗口 / 上下文 | GLFW 3.3.9 |
| 渲染 | OpenGL 3.3 Core |
| 图像 I/O | stb_image / stb_image_write |
| 构建系统 | CMake ≥ 3.16，FetchContent 自动下载所有依赖 |
| 平台 | Windows / Linux / macOS |

---

## 构建方法

### 前置条件
- CMake ≥ 3.16
- C++17 编译器（GCC 9+、Clang 10+、MSVC 2019+）
- Linux 需安装 libX11/libXrandr 等系统 GL 依赖

### 步骤

```bash
git clone https://github.com/alspxl/ImGui_imgToolAi.git
cd ImGui_imgToolAi

mkdir build && cd build
cmake ..
cmake --build . --config Release

# Linux / macOS
./ImgToolAi

# Windows (MSVC)
Release\ImgToolAi.exe
```

首次构建时 CMake 会自动通过 `FetchContent` 下载 GLFW、Dear ImGui 和 stb，无需手动处理第三方依赖。

---

## 使用说明

1. 启动程序后，通过 **File → Open** 输入图像路径打开图像（支持 PNG/JPG/BMP/TGA）。
2. 在右侧工具面板中调整参数，点击对应的 **Apply** 按钮应用效果。
3. 菜单栏 **Image** 和 **Filter** 提供快捷操作入口。
4. 鼠标滚轮缩放，拖拽平移图像。
5. 使用 **Ctrl+Z / Ctrl+Y** 撤销 / 重做（最多 20 步历史）。
6. 通过 **File → Save** 选择格式和路径保存图像。

---

## 项目结构

```
ImGui_imgToolAi/
├── CMakeLists.txt            # 顶层 CMake（FetchContent 获取 imgui/glfw/stb）
├── README.md                 # 本文档
├── .gitignore                # C++ / CMake 忽略规则
└── src/
    ├── main.cpp              # 程序入口、GLFW 窗口、主循环
    ├── app.h / app.cpp       # 主 UI 布局（菜单栏、图像视口、工具面板、状态栏）
    ├── image_manager.h/cpp   # 图像加载 / 保存 / OpenGL 纹理管理
    ├── image_processor.h/cpp # 纯 C++ 图像处理算法（15+ 种）
    ├── history_manager.h/cpp # 撤销 / 重做历史栈（最多 20 步）
    └── stb_impl.cpp          # stb_image / stb_image_write 实现单元
```

---

## 许可证

本项目代码以 MIT 许可证发布。所有第三方库（Dear ImGui、GLFW、stb）均保留其原有许可证。
