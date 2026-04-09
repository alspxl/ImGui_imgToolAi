# ImgToolAi

一款参考 [Imageshop](https://www.cnblogs.com/Imageshop/) 设计理念，基于 **Dear ImGui + OpenGL + C++17** 构建的轻量级跨平台图像处理桌面工具。

> **界面语言 / UI Language**：所有菜单、面板标题、按钮均采用 **中文 (English)** 双语同显格式。

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

### 16-bit 射线图像可视化（NEW）

#### 支持格式
- **RAW16**：原始 16-bit 灰度，可指定 宽/高/偏移头字节/字节序（Little/Big-endian）
- **TIFF 16-bit**：基线无压缩（Baseline Uncompressed）单通道 16-bit 灰度 TIFF

#### 布局模式（Layout Mode）

通过右侧工具面板的 **布局模式 (Layout Mode)** 下拉框，可在三种模式间切换：

| 模式 | 说明 |
|------|------|
| **单视图 (Single)** | 将整个缓冲区作为一幅完整图像显示，使用一套映射参数 |
| **双通道 (Two-Channel)** | 将缓冲区左半部分和右半部分解释为两个独立探测器通道；左右可独立调参，也可勾选「联动参数 (Link View Params)」共享参数 |
| **拼接 (Mosaic)** | 将缓冲区左右两半解释为同一通道的拼接展示；使用同一套参数，可选显示黄色分割线 |

#### 双通道（Two-Channel）模式说明
- 适用于同一时刻有两路探测器输出、原始数据左右拼接存储的场景（如双 CMOS 平板探测器）
- 勾选 **联动参数 (Link View Params)** 时：左右共享 Window/Level、Gamma、Invert、Colormap；
- 不勾选时：左通道和右通道各有一套独立参数，可在工具面板分别调节
- 状态栏实时显示鼠标所在区域：`[左 Left]` 或 `[右 Right]`，并显示该位置 16-bit 原始值及映射后的 8-bit 值

#### 拼接（Mosaic）模式说明
- 适用于厂商将同一传感器输出拆成左右两个视野拼接存储的场景
- 一套映射参数同时应用于整幅图像
- 可选 **显示分割线 (Show Divider)**（黄色竖线），直观标记左右边界

#### 显示映射参数（ToneMapParams）
每套映射参数集合包含：

| 参数 | 说明 |
|------|------|
| **映射模式 (Tone Map Mode)** | 自动拉伸 AutoMinMax / 百分位 Percentile / 窗宽窗位 Window-Level |
| **低/高百分位** | Percentile 模式专用，默认 P1 ~ P99 |
| **窗位 Level / 窗宽 Window** | Window-Level 模式专用（类 CT 窗宽窗位） |
| **Gamma** | 伽马校正，范围 0.1 ~ 5.0 |
| **反色 (Invert)** | 反转显示 |
| **伪彩色 (Colormap)** | 灰度 Gray / Jet / Hot |

---

### 文件支持
- **打开**：PNG、JPG、BMP、TGA（通过 stb_image，8-bit）
- **保存**：PNG、JPG、BMP、TGA（通过 stb_image_write）
- **导入 RAW16**：File → 导入 RAW16 (Import RAW16)，输入宽/高/偏移/字节序
- **导入 TIFF 16-bit**：同上对话框，文件扩展名为 `.tif`/`.tiff` 时自动使用 TIFF 读取器
- **导入 16-bit PNG**：同上对话框，自动使用 stb 的 16-bit 加载路径

### 交互体验
- 鼠标滚轮缩放图像（以鼠标位置为中心）
- 鼠标左键 / 中键拖拽平移
- 底部状态栏实时显示鼠标所指像素坐标和 RGBA 值（8-bit 模式）或原始 16-bit 值及映射值（16-bit 模式）
- **撤销 / 重做**（Ctrl+Z / Ctrl+Y，最多 20 步，仅 8-bit 模式）

---

## 中文字体配置

ImGui 默认字体不含中文字形，运行时中文标签会显示为方块。要启用中文渲染：

1. 下载一个含有 CJK 字形的 TrueType/OpenType 字体，例如：
   - **Noto Sans SC**：https://fonts.google.com/noto/specimen/Noto+Sans+SC
   - **WenQuanYi Micro Hei**：`sudo apt install fonts-wqy-microhei`（Linux）
   - 系统自带：`C:\Windows\Fonts\simhei.ttf`（Windows）或 `/System/Library/Fonts/PingFang.ttc`（macOS）
2. 将字体文件重命名为 `chinese.ttf`，放到可执行文件同级目录下的 `assets/fonts/` 中，或修改 `src/main.cpp` 中的字体路径。

> 若字体文件不存在，程序会回退到 ImGui 内置英文字体，功能不受影响，仅中文标签显示为方块。

---

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++17 |
| GUI | [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) |
| 窗口 / 上下文 | GLFW 3.3.9 |
| 渲染 | OpenGL 3.3 Core |
| 图像 I/O | stb_image / stb_image_write（含 16-bit 加载路径） |
| TIFF 读取 | 内置轻量 TIFF 读取器（`src/tiff_reader.h`，无外部依赖） |
| 构建系统 | CMake ≥ 3.16，FetchContent 自动下载所有依赖 |
| 平台 | Windows / Linux / macOS |

---

## 构建方法

### 前置条件
- CMake ≥ 3.16
- C++17 编译器（GCC 9+、Clang 10+、MSVC 2019+）
- Linux 需安装 libX11/libXrandr 等系统 GL 依赖：
  ```bash
  sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
  ```

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

### 8-bit 图像模式
1. 启动程序后，通过 **文件 (File) → 打开图像 (Open Image)** 输入图像路径。
2. 在右侧工具面板中调整参数，点击对应的 **应用 (Apply)** 按钮应用效果。
3. 使用 **Ctrl+Z / Ctrl+Y** 撤销 / 重做（最多 20 步历史）。
4. 通过 **文件 (File) → 保存图像 (Save Image)** 选择格式和路径保存图像。

### 16-bit 射线图像模式
1. 通过 **文件 (File) → 导入 RAW16 (Import RAW16)** 打开导入对话框。
2. 输入文件路径（`.raw`/`.bin`/`.tif`/`.tiff`/`.png`）。
3. 对 RAW 格式，填写正确的 **宽 (Width)** / **高 (Height)** / **头偏移 (Header Offset)** / **字节序 (Endianness)**。
4. 对 TIFF / PNG 格式，文件扩展名会被自动识别，无需填写尺寸参数。
5. 导入后在右侧面板选择 **布局模式 (Layout Mode)**，调整映射参数后实时查看效果。
6. 双通道模式下可独立调整左右通道参数，或勾选 **联动参数** 共享参数。
7. 若需切换回 8-bit 模式，点击工具面板底部的 **切换到 8-bit 视图 (Switch to 8-bit View)**。

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
    ├── image_manager.h/cpp   # 图像加载 / 保存 / OpenGL 纹理管理（8-bit）
    ├── image_processor.h/cpp # 纯 C++ 图像处理算法（15+ 种）
    ├── history_manager.h/cpp # 撤销 / 重做历史栈（最多 20 步）
    ├── raw16_state.h/cpp     # 16-bit 射线图像状态、ToneMapParams、映射管线
    ├── tiff_reader.h         # 轻量 TIFF 读取器（无压缩基线 16-bit 灰度）
    └── stb_impl.cpp          # stb_image / stb_image_write 实现单元
```

---

## 许可证

本项目代码以 MIT 许可证发布。所有第三方库（Dear ImGui、GLFW、stb）均保留其原有许可证。

