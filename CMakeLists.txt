cmake_minimum_required(VERSION 3.16)
project(ImgToolAi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch GLFW
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch Dear ImGui (docking branch)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Fetch stb
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
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

# Application sources
add_executable(ImgToolAi
    src/main.cpp
    src/app.cpp
    src/image_manager.cpp
    src/image_processor.cpp
    src/history_manager.cpp
    src/stb_impl.cpp
)
target_include_directories(ImgToolAi PRIVATE
    ${stb_SOURCE_DIR}
    src/
)
target_link_libraries(ImgToolAi PRIVATE imgui_lib OpenGL::GL)

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
    target_link_libraries(ImgToolAi PRIVATE opengl32)
endif()
