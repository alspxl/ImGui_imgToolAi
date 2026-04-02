#pragma once
// app.h
// Application class: owns the main UI layout, image viewer, and tool panels.

#include "image_manager.h"
#include "history_manager.h"

#include <string>

struct GLFWwindow;

class Application {
public:
    Application();
    ~Application();

    // Must be called once after the OpenGL context is created.
    void Init(GLFWwindow* window);

    // Render one ImGui frame.  Call between ImGui::NewFrame() / ImGui::Render().
    void Render();

private:
    // ── UI panels ────────────────────────────────────────────────────────────
    void DrawMenuBar();
    void DrawImageViewer();
    void DrawToolPanel();
    void DrawStatusBar();

    // ── Dialog helpers ───────────────────────────────────────────────────────
    void DrawOpenDialog();
    void DrawSaveDialog();

    // ── Operation helpers ────────────────────────────────────────────────────
    // Save current image state to history before modifying.
    void PushHistory();
    // Apply a processed buffer (takes ownership) as the new image.
    void ApplyProcessed(unsigned char* new_pixels, int new_w = -1, int new_h = -1);

    // ── Data ─────────────────────────────────────────────────────────────────
    GLFWwindow*    window_    = nullptr;
    ImageManager   img_mgr_;
    HistoryManager history_;

    // Image viewer state.
    float  zoom_       = 1.0f;
    float  pan_x_      = 0.0f;
    float  pan_y_      = 0.0f;
    bool   panning_    = false;
    float  last_mouse_x_ = 0.f;
    float  last_mouse_y_ = 0.f;

    // Pixel under cursor.
    int    cursor_img_x_ = -1;
    int    cursor_img_y_ = -1;

    // Dialog state.
    bool        show_open_dialog_ = false;
    bool        show_save_dialog_ = false;
    char        open_path_buf_[512] = {};
    char        save_path_buf_[512] = {};
    int         save_format_idx_    = 0; // 0=png, 1=jpg, 2=bmp, 3=tga

    // Basic adjustment parameters.
    int   brightness_   = 0;
    float contrast_     = 1.0f;
    float gamma_        = 1.0f;

    // HSL parameters.
    float hue_shift_    = 0.0f;
    float sat_scale_    = 0.0f;
    float light_shift_  = 0.0f;

    // Color balance parameters.
    float r_gain_ = 1.0f;
    float g_gain_ = 1.0f;
    float b_gain_ = 1.0f;

    // Posterize level.
    int posterize_levels_ = 4;

    // Gaussian blur radius.
    int blur_radius_ = 3;

    // Resize parameters.
    int resize_w_ = 0;
    int resize_h_ = 0;

    // Status message.
    std::string status_msg_;
};
