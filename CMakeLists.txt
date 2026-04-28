cmake_minimum_required(VERSION 3.16)
project(ImgToolAi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#?C++ O3 PU SIMD (AVX/SSE) 
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3 -march=native -ffast-math -ftree-vectorize)
elseif(MSVC)
    add_compile_options(/O2 /arch:AVX2 /fp:fast)
endif()

# ?OpenMP ?(?CPU )
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
endif()

include(FetchContent)

# Fetch GLFW
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.3.9/glfw-3.3.9.zip
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch Dear ImGui (docking branch)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/heads/docking.zip
)
FetchContent_MakeAvailable(imgui)

# Fetch stb
FetchContent_Declare(stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(stb)

# Build imgui as a static library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find TIFF
find_package(TIFF REQUIRED)

# Application sources
add_executable(ImgToolAi WIN32
    src/main.cpp
    src/app.cpp
    src/image_manager.cpp
    src/image_processor.cpp
    src/history_manager.cpp
    src/stb_impl.cpp
    src/raw16_state.cpp
    src/os_dialogs.cpp
)
target_include_directories(ImgToolAi PRIVATE
    ${stb_SOURCE_DIR}
    src/
)
target_link_libraries(ImgToolAi PRIVATE imgui_lib OpenGL::GL TIFF::TIFF)

# Platform-specific linking
if(UNIX AND NOT APPLE)
    target_link_libraries(ImgToolAi PRIVATE dl pthread)
elseif(APPLE)
    target_link_libraries(ImgToolAi PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(ImgToolAi PRIVATE opengl32 comdlg32)
endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(ImgToolAi PRIVATE OpenMP::OpenMP_CXX)
endif()

