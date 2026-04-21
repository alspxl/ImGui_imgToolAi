// main.cpp
// Application entry point: GLFW window creation, OpenGL context setup,
// Dear ImGui initialisation, and the main render loop.

#include "app.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#else
  #include <GL/gl.h>
#endif

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

static void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    // Request OpenGL 3.3 Core profile.
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    const char* glsl_version = "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* glsl_version = "#version 130";
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImgToolAi", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync.

    // Initialise Dear ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    // Slightly softer dark style.
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.f;
    style.FrameRounding     = 3.f;
    style.ScrollbarRounding = 3.f;
    style.GrabRounding      = 3.f;

    // Load Chinese fonts if on Windows
#ifdef _WIN32
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromFileTTF("c:\\windows\\fonts\\msyh.ttc", 18.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());
#endif

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    Application app;
    app.Init(window);

    // Main render loop.
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.Render();

        ImGui::Render();
        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
