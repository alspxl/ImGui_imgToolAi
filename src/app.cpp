// app.cpp
// Main application UI implementation.

#include "app.h"
#include "image_processor.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

// ── Construction / initialisation ─────────────────────────────────────────────

Application::Application() = default;
Application::~Application() = default;

void Application::Init(GLFWwindow* window) {
    window_ = window;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void Application::PushHistory() {
    if (!img_mgr_.HasImage()) return;
    history_.PushState(img_mgr_.GetWidth(), img_mgr_.GetHeight(),
                       img_mgr_.GetChannels(), img_mgr_.GetPixels());
}

void Application::ApplyProcessed(unsigned char* new_pixels, int new_w, int new_h) {
    if (!new_pixels) return;
    int w = (new_w  > 0) ? new_w  : img_mgr_.GetWidth();
    int h = (new_h  > 0) ? new_h  : img_mgr_.GetHeight();
    img_mgr_.SetPixels(new_pixels, w, h, img_mgr_.GetChannels());
}

// ── Main render entry ──────────────────────────────────────────────────────────

void Application::Render() {
    // Global keyboard shortcuts.
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        bool ctrl = io.KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Undo(w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "Undo";
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Redo(w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "Redo";
            }
        }
    }

    DrawMenuBar();
    DrawImageViewer();
    DrawToolPanel();
    DrawStatusBar();
    DrawOpenDialog();
    DrawSaveDialog();
}

// ── Menu bar ───────────────────────────────────────────────────────────────────

void Application::DrawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    // File menu.
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            show_open_dialog_ = true;
            open_path_buf_[0] = '\0';
        }
        if (ImGui::MenuItem("Save...", "Ctrl+S", false, img_mgr_.HasImage())) {
            show_save_dialog_ = true;
            // Pre-fill with the original path.
            std::snprintf(save_path_buf_, sizeof(save_path_buf_), "%s",
                          img_mgr_.GetFilePath().c_str());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    // Edit menu.
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, history_.CanUndo())) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Undo(w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "Undo";
            }
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, history_.CanRedo())) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Redo(w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "Redo";
            }
        }
        ImGui::EndMenu();
    }

    // Image menu.
    if (ImGui::BeginMenu("Image", img_mgr_.HasImage())) {
        if (ImGui::MenuItem("Grayscale")) {
            PushHistory();
            ImageProcessor::Grayscale(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            img_mgr_.UploadTexture();
            status_msg_ = "Applied: Grayscale";
        }
        if (ImGui::MenuItem("Invert Colors")) {
            PushHistory();
            ImageProcessor::Invert(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            img_mgr_.UploadTexture();
            status_msg_ = "Applied: Invert";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Flip Horizontal")) {
            PushHistory();
            auto* p = ImageProcessor::FlipHorizontal(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Flip Horizontal";
        }
        if (ImGui::MenuItem("Flip Vertical")) {
            PushHistory();
            auto* p = ImageProcessor::FlipVertical(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Flip Vertical";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Rotate 90° CW")) {
            PushHistory();
            int ow, oh;
            auto* p = ImageProcessor::Rotate(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), 90, ow, oh);
            ApplyProcessed(p, ow, oh);
            status_msg_ = "Applied: Rotate 90°";
        }
        if (ImGui::MenuItem("Rotate 180°")) {
            PushHistory();
            int ow, oh;
            auto* p = ImageProcessor::Rotate(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), 180, ow, oh);
            ApplyProcessed(p, ow, oh);
            status_msg_ = "Applied: Rotate 180°";
        }
        if (ImGui::MenuItem("Rotate 270° CW")) {
            PushHistory();
            int ow, oh;
            auto* p = ImageProcessor::Rotate(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), 270, ow, oh);
            ApplyProcessed(p, ow, oh);
            status_msg_ = "Applied: Rotate 270°";
        }
        ImGui::EndMenu();
    }

    // Filter menu.
    if (ImGui::BeginMenu("Filter", img_mgr_.HasImage())) {
        if (ImGui::MenuItem("Sharpen")) {
            PushHistory();
            auto* p = ImageProcessor::Sharpen(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Sharpen";
        }
        if (ImGui::MenuItem("Emboss")) {
            PushHistory();
            auto* p = ImageProcessor::Emboss(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Emboss";
        }
        if (ImGui::MenuItem("Edge Detection (Sobel)")) {
            PushHistory();
            auto* p = ImageProcessor::EdgeDetect(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Edge Detection";
        }
        if (ImGui::MenuItem("Dehaze")) {
            PushHistory();
            auto* p = ImageProcessor::Dehaze(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Dehaze";
        }
        ImGui::EndMenu();
    }

    // Help menu.
    if (ImGui::BeginMenu("Help")) {
        ImGui::TextDisabled("ImgToolAi v1.0");
        ImGui::Separator();
        ImGui::TextDisabled("Ctrl+O  Open image");
        ImGui::TextDisabled("Ctrl+S  Save image");
        ImGui::TextDisabled("Ctrl+Z  Undo");
        ImGui::TextDisabled("Ctrl+Y  Redo");
        ImGui::TextDisabled("Scroll  Zoom in/out");
        ImGui::TextDisabled("Drag    Pan image");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// ── Image viewer ───────────────────────────────────────────────────────────────

void Application::DrawImageViewer() {
    ImGuiIO& io = ImGui::GetIO();

    // Main dockspace / image region takes everything except tool panel and status bar.
    ImVec2 display_size = io.DisplaySize;
    float  menu_height  = ImGui::GetFrameHeight();
    float  status_h     = ImGui::GetFrameHeightWithSpacing() + 4.f;
    float  panel_w      = 280.f;

    float viewer_x = 0.f;
    float viewer_y = menu_height;
    float viewer_w = display_size.x - panel_w;
    float viewer_h = display_size.y - menu_height - status_h;

    ImGui::SetNextWindowPos(ImVec2(viewer_x, viewer_y));
    ImGui::SetNextWindowSize(ImVec2(viewer_w, viewer_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##ImageViewer", nullptr, flags);

    ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    // Draw dark checkerboard background.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    {
        float cell = 16.f;
        ImU32 c1 = IM_COL32(80, 80, 80, 255);
        ImU32 c2 = IM_COL32(60, 60, 60, 255);
        for (float cy = 0; cy < canvas_size.y; cy += cell) {
            for (float cx = 0; cx < canvas_size.x; cx += cell) {
                int row = static_cast<int>(cy / cell);
                int col = static_cast<int>(cx / cell);
                ImU32 col_val = ((row + col) % 2 == 0) ? c1 : c2;
                dl->AddRectFilled(
                    ImVec2(canvas_pos.x + cx,        canvas_pos.y + cy),
                    ImVec2(canvas_pos.x + cx + cell, canvas_pos.y + cy + cell),
                    col_val);
            }
        }
    }

    // Handle mouse input within the viewer area.
    ImGui::InvisibleButton("##canvas", canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    bool is_hovered = ImGui::IsItemHovered();

    // Zoom with scroll wheel.
    if (is_hovered) {
        float wheel = io.MouseWheel;
        if (wheel != 0.f) {
            float factor = (wheel > 0) ? 1.1f : 0.9f;
            // Zoom towards cursor position.
            float mx = io.MousePos.x - canvas_pos.x;
            float my = io.MousePos.y - canvas_pos.y;
            pan_x_ = mx - (mx - pan_x_) * factor;
            pan_y_ = my - (my - pan_y_) * factor;
            zoom_ *= factor;
            zoom_ = std::clamp(zoom_, 0.05f, 32.f);
        }
    }

    // Pan with middle mouse or left drag.
    if (is_hovered && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                        ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
        if (!panning_) {
            panning_     = true;
            last_mouse_x_ = io.MousePos.x;
            last_mouse_y_ = io.MousePos.y;
        } else {
            pan_x_ += io.MousePos.x - last_mouse_x_;
            pan_y_ += io.MousePos.y - last_mouse_y_;
            last_mouse_x_ = io.MousePos.x;
            last_mouse_y_ = io.MousePos.y;
        }
    } else {
        panning_ = false;
    }

    // Draw the image if one is loaded.
    if (img_mgr_.HasImage()) {
        float disp_w = img_mgr_.GetWidth()  * zoom_;
        float disp_h = img_mgr_.GetHeight() * zoom_;

        // Centre the image initially (pan starts at 0,0 which is top-left).
        float img_x = canvas_pos.x + pan_x_ + (canvas_size.x - disp_w) * 0.5f;
        float img_y = canvas_pos.y + pan_y_ + (canvas_size.y - disp_h) * 0.5f;

        dl->AddImage(
            (ImTextureID)(uintptr_t)(img_mgr_.GetTexture()),
            ImVec2(img_x, img_y),
            ImVec2(img_x + disp_w, img_y + disp_h));

        // Compute pixel coordinate under cursor.
        cursor_img_x_ = cursor_img_y_ = -1;
        if (is_hovered) {
            float rel_x = (io.MousePos.x - img_x) / zoom_;
            float rel_y = (io.MousePos.y - img_y) / zoom_;
            int px = static_cast<int>(rel_x);
            int py = static_cast<int>(rel_y);
            if (px >= 0 && px < img_mgr_.GetWidth() &&
                py >= 0 && py < img_mgr_.GetHeight()) {
                cursor_img_x_ = px;
                cursor_img_y_ = py;
            }
        }
    } else {
        // Draw hint text.
        const char* hint = "Open an image to get started (File > Open)";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(canvas_pos.x + (canvas_size.x - ts.x) * 0.5f,
                   canvas_pos.y + (canvas_size.y - ts.y) * 0.5f),
            IM_COL32(180, 180, 180, 180), hint);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── Tool panel ─────────────────────────────────────────────────────────────────

void Application::DrawToolPanel() {
    ImGuiIO& io = ImGui::GetIO();
    float menu_height = ImGui::GetFrameHeight();
    float status_h    = ImGui::GetFrameHeightWithSpacing() + 4.f;
    float panel_w     = 280.f;
    float panel_h     = io.DisplaySize.y - menu_height - status_h;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panel_w, menu_height));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##ToolPanel", nullptr, flags);

    bool has = img_mgr_.HasImage();

    // ── Image Info ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Image Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (has) {
            ImGui::Text("Size: %d x %d", img_mgr_.GetWidth(), img_mgr_.GetHeight());
            ImGui::Text("Channels: %d", img_mgr_.GetChannels());
            ImGui::Text("Memory: %.1f KB",
                static_cast<float>(img_mgr_.GetWidth()) * img_mgr_.GetHeight() *
                img_mgr_.GetChannels() / 1024.f);
            ImGui::Text("Zoom: %.0f%%", zoom_ * 100.f);
            if (ImGui::Button("Fit to Window")) {
                zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
            }
        } else {
            ImGui::TextDisabled("No image loaded");
        }
    }

    // ── Basic Adjustments ───────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Basic Adjustments", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!has);

        ImGui::Text("Brightness");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##brightness", &brightness_, -128, 128);
        if (ImGui::Button("Apply Brightness")) {
            PushHistory();
            ImageProcessor::AdjustBrightness(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), brightness_);
            img_mgr_.UploadTexture();
            brightness_ = 0;
            status_msg_ = "Applied: Brightness";
        }

        ImGui::Spacing();
        ImGui::Text("Contrast");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##contrast", &contrast_, 0.1f, 3.0f, "%.2f");
        if (ImGui::Button("Apply Contrast")) {
            PushHistory();
            ImageProcessor::AdjustContrast(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), contrast_);
            img_mgr_.UploadTexture();
            contrast_ = 1.0f;
            status_msg_ = "Applied: Contrast";
        }

        ImGui::Spacing();
        ImGui::Text("Gamma");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gamma", &gamma_, 0.1f, 5.0f, "%.2f");
        if (ImGui::Button("Apply Gamma")) {
            PushHistory();
            ImageProcessor::AdjustGamma(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), gamma_);
            img_mgr_.UploadTexture();
            gamma_ = 1.0f;
            status_msg_ = "Applied: Gamma";
        }

        ImGui::EndDisabled();
    }

    // ── Color Adjustments ───────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Color Adjustments")) {
        ImGui::BeginDisabled(!has);

        ImGui::Text("HSL");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hue", &hue_shift_, -180.f, 180.f, "Hue %.1f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##sat", &sat_scale_, -1.f, 1.f, "Sat %.2f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##light", &light_shift_, -0.5f, 0.5f, "Light %.2f");
        if (ImGui::Button("Apply HSL")) {
            PushHistory();
            ImageProcessor::AdjustHSL(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                hue_shift_, sat_scale_, light_shift_);
            img_mgr_.UploadTexture();
            hue_shift_ = sat_scale_ = light_shift_ = 0.f;
            status_msg_ = "Applied: HSL";
        }

        ImGui::Spacing();
        ImGui::Text("Color Balance (R/G/B Gain)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##rg", &r_gain_, 0.f, 3.f, "R %.2f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gg", &g_gain_, 0.f, 3.f, "G %.2f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##bg", &b_gain_, 0.f, 3.f, "B %.2f");
        if (ImGui::Button("Apply Color Balance")) {
            PushHistory();
            ImageProcessor::AdjustColorBalance(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                r_gain_, g_gain_, b_gain_);
            img_mgr_.UploadTexture();
            r_gain_ = g_gain_ = b_gain_ = 1.f;
            status_msg_ = "Applied: Color Balance";
        }

        ImGui::Spacing();
        ImGui::Text("Posterize Levels");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##poster", &posterize_levels_, 2, 16);
        if (ImGui::Button("Apply Posterize")) {
            PushHistory();
            ImageProcessor::Posterize(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                posterize_levels_);
            img_mgr_.UploadTexture();
            status_msg_ = "Applied: Posterize";
        }

        ImGui::EndDisabled();
    }

    // ── Filters ─────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Filters")) {
        ImGui::BeginDisabled(!has);

        ImGui::Text("Gaussian Blur Radius");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##blurr", &blur_radius_, 1, 20);
        if (ImGui::Button("Apply Gaussian Blur")) {
            PushHistory();
            auto* p = ImageProcessor::GaussianBlur(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), blur_radius_);
            ApplyProcessed(p);
            status_msg_ = "Applied: Gaussian Blur";
        }

        ImGui::Spacing();
        if (ImGui::Button("Apply Sharpen")) {
            PushHistory();
            auto* p = ImageProcessor::Sharpen(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Sharpen";
        }
        if (ImGui::Button("Apply Emboss")) {
            PushHistory();
            auto* p = ImageProcessor::Emboss(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Emboss";
        }
        if (ImGui::Button("Apply Edge Detection")) {
            PushHistory();
            auto* p = ImageProcessor::EdgeDetect(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Edge Detection";
        }
        if (ImGui::Button("Apply Dehaze")) {
            PushHistory();
            auto* p = ImageProcessor::Dehaze(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "Applied: Dehaze";
        }

        ImGui::EndDisabled();
    }

    // ── Transforms ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Transforms")) {
        ImGui::BeginDisabled(!has);

        if (has && (resize_w_ == 0 || resize_h_ == 0)) {
            resize_w_ = img_mgr_.GetWidth();
            resize_h_ = img_mgr_.GetHeight();
        }
        ImGui::Text("Resize");
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("W##rw", &resize_w_, 0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("H##rh", &resize_h_, 0, 0);
        if (resize_w_ < 1) resize_w_ = 1;
        if (resize_h_ < 1) resize_h_ = 1;
        if (ImGui::Button("Apply Resize")) {
            PushHistory();
            auto* p = ImageProcessor::Resize(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                resize_w_, resize_h_);
            ApplyProcessed(p, resize_w_, resize_h_);
            status_msg_ = "Applied: Resize";
        }

        ImGui::EndDisabled();
    }

    ImGui::End();
}

// ── Status bar ─────────────────────────────────────────────────────────────────

void Application::DrawStatusBar() {
    ImGuiIO& io = ImGui::GetIO();
    float menu_h  = ImGui::GetFrameHeight();
    float bar_h   = ImGui::GetFrameHeightWithSpacing() + 4.f;
    float bar_y   = io.DisplaySize.y - bar_h;

    ImGui::SetNextWindowPos(ImVec2(0, bar_y));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, bar_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::Begin("##StatusBar", nullptr, flags);

    if (img_mgr_.HasImage() && cursor_img_x_ >= 0) {
        const unsigned char* px = img_mgr_.GetPixels() +
            (cursor_img_y_ * img_mgr_.GetWidth() + cursor_img_x_) * img_mgr_.GetChannels();
        ImGui::Text("Pixel (%d, %d)  R:%d G:%d B:%d A:%d  |  %s",
            cursor_img_x_, cursor_img_y_,
            px[0], px[1], px[2],
            img_mgr_.GetChannels() == 4 ? px[3] : 255,
            status_msg_.c_str());
    } else {
        ImGui::Text("%s", status_msg_.empty() ? "Ready" : status_msg_.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── Dialogs ────────────────────────────────────────────────────────────────────

void Application::DrawOpenDialog() {
    if (!show_open_dialog_) return;

    ImGui::SetNextWindowSize(ImVec2(480, 120), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("Open Image##dlg", &show_open_dialog_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Enter file path (PNG / JPG / BMP / TGA):");
        ImGui::SetNextItemWidth(-1);
        bool enter = ImGui::InputText("##openpath", open_path_buf_, sizeof(open_path_buf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("Open") || enter) {
            std::string path(open_path_buf_);
            if (!path.empty()) {
                if (img_mgr_.LoadImage(path)) {
                    history_.Clear();
                    zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
                    resize_w_ = img_mgr_.GetWidth();
                    resize_h_ = img_mgr_.GetHeight();
                    status_msg_ = "Loaded: " + path;
                    // Update window title.
                    std::string title = "ImgToolAi - " + path;
                    glfwSetWindowTitle(window_, title.c_str());
                } else {
                    status_msg_ = "Failed to open: " + path;
                }
            }
            show_open_dialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) show_open_dialog_ = false;
    }
    ImGui::End();
}

void Application::DrawSaveDialog() {
    if (!show_save_dialog_) return;

    ImGui::SetNextWindowSize(ImVec2(480, 150), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("Save Image##dlg", &show_save_dialog_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Enter file path:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##savepath", save_path_buf_, sizeof(save_path_buf_));

        const char* formats[] = {"PNG", "JPG", "BMP", "TGA"};
        ImGui::Text("Format:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("##fmt", &save_format_idx_, formats, 4);

        if (ImGui::Button("Save")) {
            std::string path(save_path_buf_);
            if (!path.empty()) {
                const char* ext_map[] = {"png","jpg","bmp","tga"};
                std::string fmt = ext_map[save_format_idx_];
                if (img_mgr_.SaveImage(path, fmt)) {
                    status_msg_ = "Saved: " + path;
                } else {
                    status_msg_ = "Failed to save: " + path;
                }
            }
            show_save_dialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) show_save_dialog_ = false;
    }
    ImGui::End();
}
