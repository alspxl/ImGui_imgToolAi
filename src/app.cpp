// app.cpp
// Main application UI implementation.

#include "app.h"
#include "image_processor.h"
#include "os_dialogs.h"

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
Application::~Application() {
    raw16_.FreeTexture();
}

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
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && !mode_16bit_) {
        bool ctrl = io.KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Undo(img_mgr_.GetWidth(), img_mgr_.GetHeight(),
                              img_mgr_.GetChannels(), img_mgr_.GetPixels(),
                              w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "\xe6\x92\xa4\xe9\x94\x80 (Undo)";
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Redo(img_mgr_.GetWidth(), img_mgr_.GetHeight(),
                              img_mgr_.GetChannels(), img_mgr_.GetPixels(),
                              w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "\xe9\x87\x8d\xe5\x81\x9a (Redo)";
            }
        }
    }

    DrawMenuBar();
    if (mode_16bit_) {
        Draw16bitViewer();
        Draw16bitToolPanel();
    } else {
        DrawImageViewer();
        DrawToolPanel();
    }
    DrawStatusBar();
    DrawOpenDialog();
    DrawSaveDialog();
    DrawRaw16Dialog();
}

// ── Menu bar ───────────────────────────────────────────────────────────────────

void Application::DrawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("\xe6\x96\x87\xe4\xbb\xb6 (File)")) {
        if (ImGui::MenuItem("\xe6\x89\x93\xe5\xbc\x80\xe5\x9b\xbe\xe5\x83\x8f (Open Image)...", "Ctrl+O")) {
#ifdef _WIN32
            std::string path = OS_OpenFileDialog("Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0");
            if (!path.empty() && img_mgr_.LoadImageFile(path)) {
                history_.Clear();
                zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
                resize_w_ = img_mgr_.GetWidth();
                resize_h_ = img_mgr_.GetHeight();
                mode_16bit_ = false;
                status_msg_ = "\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd: " + path;
            }
#else
            show_open_dialog_ = true;
            open_path_buf_[0] = '\0';
#endif
        }
        if (ImGui::MenuItem("\xe5\xaf\xbc\xe5\x85\xa5 RAW16 (Import RAW16)...")) {
#ifdef _WIN32
            std::string path = OS_OpenFileDialog("RAW/16-bit\0*.raw;*.png;*.tif;*.tiff\0All Files\0*.*\0");
            if (!path.empty()) {
                mode_16bit_ = true;
                std::snprintf(raw16_path_buf_, sizeof(raw16_path_buf_), "%s", path.c_str());
                show_raw16_dialog_ = true;
            }
#else
            show_raw16_dialog_ = true;
            raw16_path_buf_[0] = '\0';
#endif
        }
        if (ImGui::MenuItem("\xe4\xbf\x9d\xe5\xad\x98\xe5\x9b\xbe\xe5\x83\x8f (Save Image)...", "Ctrl+S", false,
                            img_mgr_.HasImage() && !mode_16bit_)) {
#ifdef _WIN32
            std::string path = OS_SaveFileDialog("PNG Image\0*.png\0JPG Image\0*.jpg\0BMP Image\0*.bmp\0TGA Image\0*.tga\0");
            if (!path.empty()) {
                std::string ext = "png";
                if (path.length() >= 4) {
                    std::string fext = path.substr(path.length() - 3);
                    for (auto& c : fext) c = tolower(c);
                    if (fext == "jpg" || fext == "bmp" || fext == "tga") ext = fext;
                }
                if (img_mgr_.SaveImage(path, ext)) {
                    status_msg_ = "\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98: " + path;
                } else {
                    status_msg_ = "\xe4\xbf\x9d\xe5\xad\x98\xe5\xa4\xb1\xe8\xb4\xa5 (Save failed)";
                }
            }
#else
            show_save_dialog_ = true;
            std::snprintf(save_path_buf_, sizeof(save_path_buf_), "%s",
                          img_mgr_.GetFilePath().c_str());
#endif
        }
        ImGui::Separator();
        if (ImGui::MenuItem("\xe9\x80\x80\xe5\x87\xba (Exit)")) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("\xe7\xbc\x96\xe8\xbe\x91 (Edit)")) {
        if (ImGui::MenuItem("\xe6\x92\xa4\xe9\x94\x80 (Undo)", "Ctrl+Z", false,
                            history_.CanUndo() && !mode_16bit_)) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Undo(img_mgr_.GetWidth(), img_mgr_.GetHeight(),
                              img_mgr_.GetChannels(), img_mgr_.GetPixels(),
                              w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "\xe6\x92\xa4\xe9\x94\x80 (Undo)";
            }
        }
        if (ImGui::MenuItem("\xe9\x87\x8d\xe5\x81\x9a (Redo)", "Ctrl+Y", false,
                            history_.CanRedo() && !mode_16bit_)) {
            int w, h, c; unsigned char* data = nullptr;
            if (history_.Redo(img_mgr_.GetWidth(), img_mgr_.GetHeight(),
                              img_mgr_.GetChannels(), img_mgr_.GetPixels(),
                              w, h, c, data)) {
                img_mgr_.SetPixels(data, w, h, c);
                status_msg_ = "\xe9\x87\x8d\xe5\x81\x9a (Redo)";
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("\xe5\x9b\xbe\xe5\x83\x8f (Image)", img_mgr_.HasImage() && !mode_16bit_)) {
        if (ImGui::MenuItem("\xe7\x81\xb0\xe5\xba\xa6\xe5\x8c\x96 (Grayscale)")) {
            PushHistory();
            ImageProcessor::Grayscale(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            img_mgr_.UploadTexture();
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe7\x81\xb0\xe5\xba\xa6\xe5\x8c\x96 (Grayscale)";
        }
        if (ImGui::MenuItem("\xe5\x8f\x8d\xe8\x89\xb2 (Invert Colors)")) {
            PushHistory();
            ImageProcessor::Invert(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            img_mgr_.UploadTexture();
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe5\x8f\x8d\xe8\x89\xb2 (Invert)";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("\xe6\xb0\xb4\xe5\xb9\xb3\xe7\xbf\xbb\xe8\xbd\xac (Flip Horizontal)")) {
            PushHistory();
            auto* p = ImageProcessor::FlipHorizontal(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe6\xb0\xb4\xe5\xb9\xb3\xe7\xbf\xbb\xe8\xbd\xac (Flip H)";
        }
        if (ImGui::MenuItem("\xe5\x9e\x82\xe7\x9b\xb4\xe7\xbf\xbb\xe8\xbd\xac (Flip Vertical)")) {
            PushHistory();
            auto* p = ImageProcessor::FlipVertical(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe5\x9e\x82\xe7\x9b\xb4\xe7\xbf\xbb\xe8\xbd\xac (Flip V)";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("\xe9\xa1\xba\xe6\x97\xb6\xe9\x92\x91" "20\xc2\xb0 (Rotate 90\xc2\xb0 CW)")) {
            PushHistory();
            int ow, oh;
            auto* p = ImageProcessor::Rotate(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), 90, ow, oh);
            ApplyProcessed(p, ow, oh);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe6\x97\x8b\xe8\xbd\xac 90\xc2\xb0";
        }
        if (ImGui::MenuItem("\xe6\x97\x8b\xe8\xbd\xac 180\xc2\xb0 (Rotate 180\xc2\xb0)")) {
            PushHistory();
            int ow, oh;
            auto* p = ImageProcessor::Rotate(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), 180, ow, oh);
            ApplyProcessed(p, ow, oh);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe6\x97\x8b\xe8\xbd\xac 180\xc2\xb0";
        }
        if (ImGui::MenuItem("\xe9\x80\x86\xe6\x97\xb6\xe9\x92\x91" "20\xc2\xb0 (Rotate 270\xc2\xb0 CW)")) {
            PushHistory();
            int ow, oh;
            auto* p = ImageProcessor::Rotate(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), 270, ow, oh);
            ApplyProcessed(p, ow, oh);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe6\x97\x8b\xe8\xbd\xac 270\xc2\xb0";
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("\xe6\xbb\xa4\xe9\x95\x9c (Filter)", img_mgr_.HasImage() && !mode_16bit_)) {
        if (ImGui::MenuItem("\xe9\x94\x90\xe5\x8c\x96 (Sharpen)")) {
            PushHistory();
            auto* p = ImageProcessor::Sharpen(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe9\x94\x90\xe5\x8c\x96 (Sharpen)";
        }
        if (ImGui::MenuItem("\xe6\xb5\xae\xe9\x9b\x95 (Emboss)")) {
            PushHistory();
            auto* p = ImageProcessor::Emboss(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe6\xb5\xae\xe9\x9b\x95 (Emboss)";
        }
        if (ImGui::MenuItem("\xe8\xbe\xb9\xe7\xbc\x98\xe6\xa3\x80\xe6\xb5\x8b Sobel (Edge Detection)")) {
            PushHistory();
            auto* p = ImageProcessor::EdgeDetect(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe8\xbe\xb9\xe7\xbc\x98\xe6\xa3\x80\xe6\xb5\x8b (Edge)";
        }
        if (ImGui::MenuItem("\xe5\x8e\xbb\xe9\x9b\xbe (Dehaze)")) {
            PushHistory();
            auto* p = ImageProcessor::Dehaze(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe5\x8e\xbb\xe9\x9b\xbe (Dehaze)";
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("\xe5\xb8\xae\xe5\x8a\xa9 (Help)")) {
        ImGui::TextDisabled("ImgToolAi v1.1");
        ImGui::Separator();
        ImGui::TextDisabled("Ctrl+O  \xe6\x89\x93\xe5\xbc\x80\xe5\x9b\xbe\xe5\x83\x8f (Open image)");
        ImGui::TextDisabled("Ctrl+S  \xe4\xbf\x9d\xe5\xad\x98\xe5\x9b\xbe\xe5\x83\x8f (Save image)");
        ImGui::TextDisabled("Ctrl+Z  \xe6\x92\xa4\xe9\x94\x80 (Undo)");
        ImGui::TextDisabled("Ctrl+Y  \xe9\x87\x8d\xe5\x81\x9a (Redo)");
        ImGui::TextDisabled("\xe6\xbb\x9a\xe8\xbd\xae (Scroll)  \xe7\xbc\xa9\xe6\x94\xbe (Zoom)");
        ImGui::TextDisabled("\xe6\x8b\x96\xe6\x8b\xbd (Drag)    \xe5\xb9\xb3\xe7\xa7\xbb (Pan)");
        ImGui::Separator();
        ImGui::TextDisabled("\xe6\x94\xaf\xe6\x8c\x81\xe6\xa0\xbc\xe5\xbc\x8f (Formats):");
        ImGui::TextDisabled("  8-bit: PNG/JPG/BMP/TGA");
        ImGui::TextDisabled("  16-bit RAW: \xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe5\xaf\xbc\xe5\x85\xa5 (custom import)");
        ImGui::TextDisabled("  16-bit TIFF: \xe5\x9f\xba\xe7\xba\xbf\xe6\x97\xa0\xe5\x8e\x8b\xe7\xbc\xa9 (baseline uncompressed)");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// ── Image viewer (8-bit) ───────────────────────────────────────────────────────

void Application::DrawImageViewer() {
    ImGuiIO& io = ImGui::GetIO();

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

    ImGui::InvisibleButton("##canvas", canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    bool is_hovered = ImGui::IsItemHovered();

    if (is_hovered) {
        float wheel = io.MouseWheel;
        if (wheel != 0.f) {
            float factor = (wheel > 0) ? 1.1f : 0.9f;
            float mx = io.MousePos.x - canvas_pos.x;
            float my = io.MousePos.y - canvas_pos.y;
            pan_x_ = mx - (mx - pan_x_) * factor;
            pan_y_ = my - (my - pan_y_) * factor;
            zoom_ *= factor;
            zoom_ = std::clamp(zoom_, 0.05f, 32.f);
        }
    }

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

    if (img_mgr_.HasImage()) {
        float disp_w = img_mgr_.GetWidth()  * zoom_;
        float disp_h = img_mgr_.GetHeight() * zoom_;

        float img_x = canvas_pos.x + pan_x_ + (canvas_size.x - disp_w) * 0.5f;
        float img_y = canvas_pos.y + pan_y_ + (canvas_size.y - disp_h) * 0.5f;

        dl->AddImage(
            (ImTextureID)(uintptr_t)(img_mgr_.GetTexture()),
            ImVec2(img_x, img_y),
            ImVec2(img_x + disp_w, img_y + disp_h));

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
        const char* hint = "\xe6\x89\x93\xe5\xbc\x80\xe5\x9b\xbe\xe5\x83\x8f\xe5\xbc\x80\xe5\xa7\x8b\xe4\xbd\xbf\xe7\x94\xa8 (Open an image �?File > Open Image)";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(canvas_pos.x + (canvas_size.x - ts.x) * 0.5f,
                   canvas_pos.y + (canvas_size.y - ts.y) * 0.5f),
            IM_COL32(180, 180, 180, 180), hint);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── 16-bit viewer ──────────────────────────────────────────────────────────────

void Application::Draw16bitViewer() {
    ImGuiIO& io = ImGui::GetIO();

    float menu_height = ImGui::GetFrameHeight();
    float status_h    = ImGui::GetFrameHeightWithSpacing() + 4.f;
    float panel_w     = 300.f;

    float viewer_x = 0.f;
    float viewer_y = menu_height;
    float viewer_w = io.DisplaySize.x - panel_w;
    float viewer_h = io.DisplaySize.y - menu_height - status_h;

    ImGui::SetNextWindowPos(ImVec2(viewer_x, viewer_y));
    ImGui::SetNextWindowSize(ImVec2(viewer_w, viewer_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##16bitViewer", nullptr, flags);

    ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    {
        float cell = 16.f;
        ImU32 c1 = IM_COL32(50, 50, 50, 255);
        ImU32 c2 = IM_COL32(35, 35, 35, 255);
        for (float cy = 0; cy < canvas_size.y; cy += cell)
            for (float cx = 0; cx < canvas_size.x; cx += cell) {
                int row = static_cast<int>(cy / cell);
                int col = static_cast<int>(cx / cell);
                dl->AddRectFilled(
                    ImVec2(canvas_pos.x + cx,        canvas_pos.y + cy),
                    ImVec2(canvas_pos.x + cx + cell, canvas_pos.y + cy + cell),
                    ((row + col) % 2 == 0) ? c1 : c2);
            }
    }

    ImGui::InvisibleButton("##canvas16", canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool is_hovered = ImGui::IsItemHovered();

    if (is_hovered) {
        float wheel = io.MouseWheel;
        if (wheel != 0.f) {
            float factor = (wheel > 0) ? 1.1f : 0.9f;
            float mx = io.MousePos.x - canvas_pos.x;
            float my = io.MousePos.y - canvas_pos.y;
            pan_x_ = mx - (mx - pan_x_) * factor;
            pan_y_ = my - (my - pan_y_) * factor;
            zoom_ *= factor;
            zoom_ = std::clamp(zoom_, 0.05f, 32.f);
        }
    }
    if (is_hovered && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                        ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
        if (!panning_) {
            panning_      = true;
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

    if (raw16_.has_data()) {
        raw16_.MapAndUpload();

        float disp_w = raw16_.width  * zoom_;
        float disp_h = raw16_.height * zoom_;

        float img_x = canvas_pos.x + pan_x_ + (canvas_size.x - disp_w) * 0.5f;
        float img_y = canvas_pos.y + pan_y_ + (canvas_size.y - disp_h) * 0.5f;

        dl->AddImage(
            (ImTextureID)(uintptr_t)(raw16_.tex_display),
            ImVec2(img_x, img_y),
            ImVec2(img_x + disp_w, img_y + disp_h));

        cursor_img_x_ = cursor_img_y_ = -1;
        cursor_region_ = 0;
        if (is_hovered) {
            float rel_x = (io.MousePos.x - img_x) / zoom_;
            float rel_y = (io.MousePos.y - img_y) / zoom_;
            int px = static_cast<int>(rel_x);
            int py = static_cast<int>(rel_y);
            if (px >= 0 && px < raw16_.width &&
                py >= 0 && py < raw16_.height) {
                cursor_img_x_ = px;
                cursor_img_y_ = py;
                if (raw16_.layout != LayoutMode::Single && px >= raw16_.width / 2)
                    cursor_region_ = 2;
                else
                    cursor_region_ = 1;
            }
        }
    } else {
        const char* hint = "\xe5\xaf\xbc\xe5\x85\xa5 RAW16 \xe6\x88\x96 TIFF 16-bit \xe5\x9b\xbe\xe5\x83\x8f (Import RAW16 or TIFF �?File > Import RAW16)";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(canvas_pos.x + (canvas_size.x - ts.x) * 0.5f,
                   canvas_pos.y + (canvas_size.y - ts.y) * 0.5f),
            IM_COL32(180, 180, 180, 180), hint);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── ToneMap parameter panel helper ────────────────────────────────────────────

void Application::DrawToneMapPanel(ToneMapParams& p, const char* id_prefix, bool /*show_header*/) {
    char label[128];

    // Tone-map mode.
    const char* modes[] = {
        "\xe8\x87\xaa\xe5\x8a\xa8\xe6\x8b\x89\xe4\xbc\xb8 (Auto Min/Max)",
        "\xe7\x99\xbe\xe5\x88\x86\xe4\xbd\x8d (Percentile)",
        "\xe7\xaa\x97\xe5\xae\xbd\xe7\xaa\x97\xe4\xbd\x8d (Window/Level)"
    };
    int mode_idx = static_cast<int>(p.mode);
    std::snprintf(label, sizeof(label), "##tm_mode_%s", id_prefix);
    ImGui::Text("\xe6\x98\xa0\xe5\xb0\x84\xe6\xa8\xa1\xe5\xbc\x8f (Tone Map Mode)");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo(label, &mode_idx, modes, 3)) {
        p.mode  = static_cast<ToneMapMode>(mode_idx);
        p.dirty = true;
    }

    if (p.mode == ToneMapMode::Percentile) {
        std::snprintf(label, sizeof(label), "##plo_%s", id_prefix);
        ImGui::Text("\xe4\xbd\x8e\xe7\x99\xbe\xe5\x88\x86\xe4\xbd\x8d (Low %%)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat(label, &p.p_low, 0.f, 10.f, "%.1f%%"))
            p.dirty = true;
        std::snprintf(label, sizeof(label), "##phi_%s", id_prefix);
        ImGui::Text("\xe9\xab\x98\xe7\x99\xbe\xe5\x88\x86\xe4\xbd\x8d (High %%)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat(label, &p.p_high, 90.f, 100.f, "%.1f%%"))
            p.dirty = true;
    }

    if (p.mode == ToneMapMode::WindowLevel) {
        std::snprintf(label, sizeof(label), "##wl_lv_%s", id_prefix);
        ImGui::Text("\xe7\xaa\x97\xe4\xbd\x8d (Level)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat(label, &p.level, 0.f, 65535.f, "%.0f"))
            p.dirty = true;
        std::snprintf(label, sizeof(label), "##wl_wn_%s", id_prefix);
        ImGui::Text("\xe7\xaa\x97\xe5\xae\xbd (Window)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat(label, &p.window, 1.f, 65535.f, "%.0f"))
            p.dirty = true;
    }

    std::snprintf(label, sizeof(label), "##gamma_%s", id_prefix);
    ImGui::Text("Gamma");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat(label, &p.gamma, 0.1f, 5.0f, "%.2f"))
        p.dirty = true;

    std::snprintf(label, sizeof(label), "\xe5\x8f\x8d\xe8\x89\xb2 (Invert)##inv_%s", id_prefix);
    if (ImGui::Checkbox(label, &p.invert))
        p.dirty = true;

    const char* cmaps[] = { "\xe7\x81\xb0\xe5\xba\xa6 (Gray)", "Jet", "Hot" };
    int cmap_idx = static_cast<int>(p.colormap);
    std::snprintf(label, sizeof(label), "##cmap_%s", id_prefix);
    ImGui::Text("\xe4\xbc\xaa\xe5\xbd\xa9\xe8\x89\xb2 (Colormap)");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo(label, &cmap_idx, cmaps, 3)) {
        p.colormap = static_cast<ColormapType>(cmap_idx);
        p.dirty    = true;
    }
}

// ── 16-bit tool panel ──────────────────────────────────────────────────────────

void Application::Draw16bitToolPanel() {
    ImGuiIO& io = ImGui::GetIO();
    float menu_height = ImGui::GetFrameHeight();
    float status_h    = ImGui::GetFrameHeightWithSpacing() + 4.f;
    float panel_w     = 300.f;
    float panel_h     = io.DisplaySize.y - menu_height - status_h;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panel_w, menu_height));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##16bitPanel", nullptr, flags);

    bool has = raw16_.has_data();

    // 图像信息 (Image Info)
    if (ImGui::CollapsingHeader("\xe5\x9b\xbe\xe5\x83\x8f\xe4\xbf\xa1\xe6\x81\xaf (Image Info)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (has) {
            ImGui::Text("\xe5\xb0\xba\xe5\xaf\xb8 (Size): %d x %d", raw16_.width, raw16_.height);
            ImGui::Text("\xe5\x83\x8f\xe7\xb4\xa0\xe6\x95\xb0 (Pixels): %llu",
                        (unsigned long long)(raw16_.data.size()));
            ImGui::Text("\xe7\xbc\xa9\xe6\x94\xbe (Zoom): %.0f%%", zoom_ * 100.f);
            if (ImGui::Button("\xe9\x80\x82\xe5\x90\x88\xe7\xaa\x97\xe5\x8f\xa3 (Fit to Window)")) {
                zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
            }
        } else {
            ImGui::TextDisabled("\xe6\x9c\xaa\xe5\x8a\xa0\xe8\xbd\xbd 16-bit \xe5\x9b\xbe\xe5\x83\x8f (No 16-bit image loaded)");
        }
    }

    // 布局模式 (Layout Mode)
    if (ImGui::CollapsingHeader("\xe5\xb8\x83\xe5\xb1\x80\xe6\xa8\xa1\xe5\xbc\x8f (Layout Mode)", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* layouts[] = {
            "\xe5\x8d\x95\xe8\xa7\x86\xe5\x9b\xbe (Single)",
            "\xe5\x8f\x8c\xe9\x80\x9a\xe9\x81\x93 (Two-Channel)",
            "\xe6\x8b\xbc\xe6\x8e\xa5 (Mosaic)"
        };
        int layout_idx = static_cast<int>(raw16_.layout);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##layout", &layout_idx, layouts, 3)) {
            raw16_.layout = static_cast<LayoutMode>(layout_idx);
            raw16_.MarkDirty();
        }

        if (raw16_.layout == LayoutMode::TwoChannel) {
            ImGui::Spacing();
            if (ImGui::Checkbox("\xe8\x81\x94\xe5\x8a\xa8\xe5\x8f\x82\xe6\x95\xb0 (Link View Params)", &raw16_.link_params))
                raw16_.MarkDirty();
        }
        if (raw16_.layout != LayoutMode::Single) {
            if (ImGui::Checkbox("\xe6\x98\xbe\xe7\xa4\xba\xe5\x88\x86\xe5\x89\xb2\xe7\xba\xbf (Show Divider)", &raw16_.show_divider))
                raw16_.MarkDirty();
        }
    }

    // Display params
    if (raw16_.layout == LayoutMode::Single || raw16_.layout == LayoutMode::Mosaic) {
        if (ImGui::CollapsingHeader("\xe6\x98\xbe\xe7\xa4\xba\xe5\x8f\x82\xe6\x95\xb0 (Display Parameters)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginDisabled(!has);
            DrawToneMapPanel(raw16_.params_left, "L", false);
            ImGui::EndDisabled();
        }
    } else {
        if (raw16_.link_params) {
            if (ImGui::CollapsingHeader("\xe6\x98\xbe\xe7\xa4\xba\xe5\x8f\x82\xe6\x95\xb0-\xe8\x81\x94\xe5\x8a\xa8 (Params �?Linked)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::BeginDisabled(!has);
                DrawToneMapPanel(raw16_.params_left, "LR", false);
                ImGui::EndDisabled();
            }
        } else {
            if (ImGui::CollapsingHeader("\xe5\xb7\xa6\xe9\x80\x9a\xe9\x81\x93 (Left Channel)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::BeginDisabled(!has);
                DrawToneMapPanel(raw16_.params_left, "L", false);
                ImGui::EndDisabled();
            }
            if (ImGui::CollapsingHeader("\xe5\x8f\xb3\xe9\x80\x9a\xe9\x81\x93 (Right Channel)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::BeginDisabled(!has);
                DrawToneMapPanel(raw16_.params_right, "R", false);
                ImGui::EndDisabled();
            }
        }
    }

    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0 8-bit \xe8\xa7\x86\xe5\x9b\xbe (Switch to 8-bit View)")) {
        mode_16bit_ = false;
        status_msg_ = "\xe5\xb7\xb2\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0 8-bit \xe6\xa8\xa1\xe5\xbc\x8f (Switched to 8-bit mode)";
    }

    ImGui::End();
}

// ── Tool panel (8-bit) ─────────────────────────────────────────────────────────

void Application::DrawToolPanel() {
    bool any_slider_changed = false;
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

    if (ImGui::CollapsingHeader("\xe5\x9b\xbe\xe5\x83\x8f\xe4\xbf\xa1\xe6\x81\xaf (Image Info)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (has) {
            ImGui::Text("\xe5\xb0\xba\xe5\xaf\xb8 (Size): %d x %d", img_mgr_.GetWidth(), img_mgr_.GetHeight());
            ImGui::Text("\xe9\x80\x9a\xe9\x81\x93\xe6\x95\xb0 (Channels): %d", img_mgr_.GetChannels());
            ImGui::Text("\xe5\x86\x85\xe5\xad\x98 (Memory): %.1f KB",
                static_cast<float>(img_mgr_.GetWidth()) * img_mgr_.GetHeight() *
                img_mgr_.GetChannels() / 1024.f);
            ImGui::Text("\xe7\xbc\xa9\xe6\x94\xbe (Zoom): %.0f%%", zoom_ * 100.f);
            if (ImGui::Button("\xe9\x80\x82\xe5\x90\x88\xe7\xaa\x97\xe5\x8f\xa3 (Fit to Window)")) {
                zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
            }
        } else {
            ImGui::TextDisabled("\xe6\x9c\xaa\xe5\x8a\xa0\xe8\xbd\xbd\xe5\x9b\xbe\xe5\x83\x8f (No image loaded)");
        }
    }

    if (ImGui::CollapsingHeader("\xe5\x9f\xba\xe7\xa1\x80\xe8\xb0\x83\xe6\x95\xb4 (Basic Adjustments)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!has);

        ImGui::Text("\xe4\xba\xae\xe5\xba\xa6 (Brightness)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##brightness", &brightness_, -128, 128); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe4\xba\xae\xe5\xba\xa6 (Apply Brightness)")) {
            PushHistory();
            ImageProcessor::AdjustBrightness(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), brightness_);
            img_mgr_.UploadTexture();
            brightness_ = 0;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe4\xba\xae\xe5\xba\xa6 (Brightness)";
        }

        ImGui::Spacing();
        ImGui::Text("\xe5\xaf\xb9\xe6\xaf\x94\xe5\xba\xa6 (Contrast)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##contrast", &contrast_, 0.1f, 3.0f, "%.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe5\xaf\xb9\xe6\xaf\x94\xe5\xba\xa6 (Apply Contrast)")) {
            PushHistory();
            ImageProcessor::AdjustContrast(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), contrast_);
            img_mgr_.UploadTexture();
            contrast_ = 1.0f;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe5\xaf\xb9\xe6\xaf\x94\xe5\xba\xa6 (Contrast)";
        }

        ImGui::Spacing();
        ImGui::Text("Gamma");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gamma", &gamma_, 0.1f, 5.0f, "%.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8 Gamma (Apply Gamma)")) {
            PushHistory();
            ImageProcessor::AdjustGamma(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), gamma_);
            img_mgr_.UploadTexture();
            gamma_ = 1.0f;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: Gamma";
        }

        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("\xe8\x89\xb2\xe5\xbd\xa9\xe8\xb0\x83\xe6\x95\xb4 (Color Adjustments)")) {
        ImGui::BeginDisabled(!has);

        ImGui::Text("HSL");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hue", &hue_shift_, -180.f, 180.f, "\xe8\x89\xb2\xe7\x9b\xb8 (Hue) %.1f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##sat", &sat_scale_, -1.f, 1.f, "\xe9\xa5\xb1\xe5\x92\x8c\xe5\xba\xa6 (Sat) %.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##light", &light_shift_, -0.5f, 0.5f, "\xe4\xba\xae\xe5\xba\xa6 (Light) %.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8 HSL (Apply HSL)")) {
            PushHistory();
            ImageProcessor::AdjustHSL(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                hue_shift_, sat_scale_, light_shift_);
            img_mgr_.UploadTexture();
            hue_shift_ = sat_scale_ = light_shift_ = 0.f;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: HSL";
        }

        ImGui::Spacing();
        ImGui::Text("\xe8\x89\xb2\xe5\xbd\xa9\xe5\xb9\xb3\xe8\xa1\xa1 (Color Balance R/G/B)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##rg", &r_gain_, 0.f, 3.f, "R %.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gg", &g_gain_, 0.f, 3.f, "G %.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##bg", &b_gain_, 0.f, 3.f, "B %.2f"); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe8\x89\xb2\xe5\xbd\xa9\xe5\xb9\xb3\xe8\xa1\xa1 (Apply Color Balance)")) {
            PushHistory();
            ImageProcessor::AdjustColorBalance(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                r_gain_, g_gain_, b_gain_);
            img_mgr_.UploadTexture();
            r_gain_ = g_gain_ = b_gain_ = 1.f;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe8\x89\xb2\xe5\xbd\xa9\xe5\xb9\xb3\xe8\xa1\xa1 (Color Balance)";
        }

        ImGui::Spacing();
        ImGui::Text("\xe8\x89\xb2\xe9\x98\xb6 (Posterize Levels)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##poster", &posterize_levels_, 2, 16); any_slider_changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe8\x89\xb2\xe9\x98\xb6 (Apply Posterize)")) {
            PushHistory();
            ImageProcessor::Posterize(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                posterize_levels_);
            img_mgr_.UploadTexture();
            posterize_levels_ = 4;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe8\x89\xb2\xe9\x98\xb6 (Posterize)";
        }

        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("\xe6\xbb\xa4\xe9\x95\x9c (Filters)")) {
        ImGui::BeginDisabled(!has);

        ImGui::Text("\xe9\xab\x98\xe6\x96\xaf\xe6\xa8\xa1\xe7\xb3\x8a\xe5\x8d\x8a\xe5\xbe\x84 (Gaussian Blur Radius)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##blurr", &blur_radius_, 1, 20);
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe9\xab\x98\xe6\x96\xaf\xe6\xa8\xa1\xe7\xb3\x8a (Apply Gaussian Blur)")) {
            PushHistory();
            auto* p = ImageProcessor::GaussianBlur(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), blur_radius_);
            ApplyProcessed(p);
            blur_radius_ = 3;
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe9\xab\x98\xe6\x96\xaf\xe6\xa8\xa1\xe7\xb3\x8a (Gaussian Blur)";
        }

        ImGui::Spacing();
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe9\x94\x90\xe5\x8c\x96 (Apply Sharpen)")) {
            PushHistory();
            auto* p = ImageProcessor::Sharpen(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe9\x94\x90\xe5\x8c\x96 (Sharpen)";
        }
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe6\xb5\xae\xe9\x9b\x95 (Apply Emboss)")) {
            PushHistory();
            auto* p = ImageProcessor::Emboss(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe6\xb5\xae\xe9\x9b\x95 (Emboss)";
        }
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe8\xbe\xb9\xe7\xbc\x98\xe6\xa3\x80\xe6\xb5\x8b (Apply Edge Detection)")) {
            PushHistory();
            auto* p = ImageProcessor::EdgeDetect(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe8\xbe\xb9\xe7\xbc\x98\xe6\xa3\x80\xe6\xb5\x8b (Edge Detection)";
        }
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe5\x8e\xbb\xe9\x9b\xbe (Apply Dehaze)")) {
            PushHistory();
            auto* p = ImageProcessor::Dehaze(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels());
            ApplyProcessed(p);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe5\x8e\xbb\xe9\x9b\xbe (Dehaze)";
        }

        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("\xe5\x87\xa0\xe4\xbd\x95\xe5\x8f\x98\xe6\x8d\xa2 (Transforms)")) {
        ImGui::BeginDisabled(!has);

        if (has && (resize_w_ == 0 || resize_h_ == 0)) {
            resize_w_ = img_mgr_.GetWidth();
            resize_h_ = img_mgr_.GetHeight();
        }
        ImGui::Text("\xe7\xbc\xa9\xe6\x94\xbe\xe5\xb0\xba\xe5\xaf\xb8 (Resize)");
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("W##rw", &resize_w_, 0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("H##rh", &resize_h_, 0, 0);
        if (resize_w_ < 1) resize_w_ = 1;
        if (resize_h_ < 1) resize_h_ = 1;
        if (ImGui::Button("\xe5\xba\x94\xe7\x94\xa8\xe7\xbc\xa9\xe6\x94\xbe (Apply Resize)")) {
            PushHistory();
            auto* p = ImageProcessor::Resize(img_mgr_.GetPixels(),
                img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(),
                resize_w_, resize_h_);
            ApplyProcessed(p, resize_w_, resize_h_);
            status_msg_ = "\xe5\xb7\xb2\xe5\xba\x94\xe7\x94\xa8: \xe7\xbc\xa9\xe6\x94\xbe (Resize)";
        }

        ImGui::EndDisabled();
    }

    if (img_mgr_.HasImage()) {
        ImGui::Spacing(); ImGui::Separator();
        bool prev_active = preview_active_;
        ImGui::Checkbox("\xe5\xae\x9e\xe6\x97\xb6\xe9\xa2\x84\xe8\xa7\x88 (Live Preview)", &preview_active_);
        if (prev_active && !preview_active_) {
            img_mgr_.UploadTexture(); // Restore original
        }
        
        if (preview_active_ && any_slider_changed) {
            unsigned char* p = img_mgr_.ClonePixels();
            bool changed = false;
            
            if (brightness_ != 0) { ImageProcessor::AdjustBrightness(p, img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), brightness_); changed = true; }
            if (contrast_ != 1.0f) { ImageProcessor::AdjustContrast(p, img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), contrast_); changed = true; }
            if (gamma_ != 1.0f) { ImageProcessor::AdjustGamma(p, img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), gamma_); changed = true; }
            if (hue_shift_ != 0.f || sat_scale_ != 0.f || light_shift_ != 0.f) { ImageProcessor::AdjustHSL(p, img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), hue_shift_, sat_scale_, light_shift_); changed = true; }
            if (r_gain_ != 1.0f || g_gain_ != 1.0f || b_gain_ != 1.0f) { ImageProcessor::AdjustColorBalance(p, img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), r_gain_, g_gain_, b_gain_); changed = true; }
            if (posterize_levels_ != 4) { ImageProcessor::Posterize(p, img_mgr_.GetWidth(), img_mgr_.GetHeight(), img_mgr_.GetChannels(), posterize_levels_); changed = true; }

            if (changed) {
                img_mgr_.UpdateTextureFrom(p);
            } else {
                img_mgr_.UploadTexture();
            }
            delete[] p;
        }
    }

    ImGui::End();
}

// ── Status bar ─────────────────────────────────────────────────────────────────

void Application::DrawStatusBar() {
    ImGuiIO& io = ImGui::GetIO();
    float bar_h = ImGui::GetFrameHeightWithSpacing() + 4.f;
    float bar_y = io.DisplaySize.y - bar_h;

    ImGui::SetNextWindowPos(ImVec2(0, bar_y));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, bar_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::Begin("##StatusBar", nullptr, flags);

    if (mode_16bit_) {
        // 16-bit pixel info
        if (raw16_.has_data() && cursor_img_x_ >= 0 && cursor_img_y_ >= 0) {
            int px = cursor_img_x_;
            int py = cursor_img_y_;
            uint16_t raw_val = raw16_.data[static_cast<size_t>(py) * raw16_.width + px];

            // Determine region
            const char* region_str = "";
            const ToneMapParams* p_used = &raw16_.params_left;
            if (raw16_.layout != LayoutMode::Single) {
                if (cursor_region_ == 2) {
                    region_str = "  [\xe5\x8f\xb3 Right]";
                    if (!raw16_.link_params)
                        p_used = &raw16_.params_right;
                } else {
                    region_str = "  [\xe5\xb7\xa6 Left]";
                }
            }

            // Compute mapped value for status display
            float wl, wh;
            // Use the full buffer for window calculation (avoids half-side extraction cost)
            ComputeWindowLH(*p_used, raw16_.data.data(), static_cast<int>(raw16_.data.size()), wl, wh);
            float t = (static_cast<float>(raw_val) - wl) / (wh - wl);
            t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
            int mapped8 = static_cast<int>(t * 255.f + 0.5f);

            ImGui::Text("\xe5\x83\x8f\xe7\xb4\xa0 (Pixel) (%d, %d)%s  "
                        "\xe5\x8e\x9f\xe5\xa7\x8b\xe5\x80\xbc (Raw): %u  "
                        "\xe6\x98\xa0\xe5\xb0\x84\xe5\x80\xbc (Mapped): %d  |  %s",
                px, py, region_str, (unsigned)raw_val, mapped8,
                status_msg_.c_str());
        } else {
            ImGui::Text("16-bit \xe6\xa8\xa1\xe5\xbc\x8f (mode)  |  %s",
                status_msg_.empty() ? "\xe5\xb0\xb1\xe7\xbb\xaa (Ready)" : status_msg_.c_str());
        }
    } else {
        // 8-bit pixel info
        if (img_mgr_.HasImage() && cursor_img_x_ >= 0) {
            const unsigned char* pix = img_mgr_.GetPixels() +
                (cursor_img_y_ * img_mgr_.GetWidth() + cursor_img_x_) * img_mgr_.GetChannels();
            int ch = img_mgr_.GetChannels();
            ImGui::Text("\xe5\x83\x8f\xe7\xb4\xa0 (Pixel) (%d, %d)  R:%d G:%d B:%d A:%d  |  %s",
                cursor_img_x_, cursor_img_y_,
                ch >= 1 ? pix[0] : 0,
                ch >= 2 ? pix[1] : 0,
                ch >= 3 ? pix[2] : 0,
                ch >= 4 ? pix[3] : 255,
                status_msg_.c_str());
        } else {
            ImGui::Text("%s", status_msg_.empty() ? "\xe5\xb0\xb1\xe7\xbb\xaa (Ready)" : status_msg_.c_str());
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── Dialogs ────────────────────────────────────────────────────────────────────

void Application::DrawOpenDialog() {
    if (!show_open_dialog_) return;

    ImGui::SetNextWindowSize(ImVec2(500, 140), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("\xe6\x89\x93\xe5\xbc\x80\xe5\x9b\xbe\xe5\x83\x8f (Open Image)##dlg", &show_open_dialog_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("\xe8\xbe\x93\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84 (Enter file path) �?PNG / JPG / BMP / TGA:");
        ImGui::SetNextItemWidth(-1);
        bool enter = ImGui::InputText("##openpath", open_path_buf_, sizeof(open_path_buf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("\xe6\x89\x93\xe5\xbc\x80 (Open)") || enter) {
            std::string path(open_path_buf_);
            if (!path.empty()) {
                if (img_mgr_.LoadImageFile(path)) {
                    history_.Clear();
                    zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
                    resize_w_ = img_mgr_.GetWidth();
                    resize_h_ = img_mgr_.GetHeight();
                    mode_16bit_ = false;
                    status_msg_ = "\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd (Loaded): " + path;
                    std::string title = "ImgToolAi - " + path;
                    glfwSetWindowTitle(window_, title.c_str());
                } else {
                    status_msg_ = "\xe6\x89\x93\xe5\xbc\x80\xe5\xa4\xb1\xe8\xb4\xa5 (Failed to open): " + path;
                }
            }
            show_open_dialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("\xe5\x8f\x96\xe6\xb6\x88 (Cancel)")) show_open_dialog_ = false;
    }
    ImGui::End();
}

void Application::DrawSaveDialog() {
    if (!show_save_dialog_) return;

    ImGui::SetNextWindowSize(ImVec2(500, 160), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("\xe4\xbf\x9d\xe5\xad\x98\xe5\x9b\xbe\xe5\x83\x8f (Save Image)##dlg", &show_save_dialog_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("\xe8\xbe\x93\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84 (Enter file path):");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##savepath", save_path_buf_, sizeof(save_path_buf_));

        const char* formats[] = {"PNG", "JPG", "BMP", "TGA"};
        ImGui::Text("\xe6\xa0\xbc\xe5\xbc\x8f (Format):");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("##fmt", &save_format_idx_, formats, 4);

        if (ImGui::Button("\xe4\xbf\x9d\xe5\xad\x98 (Save)")) {
            std::string path(save_path_buf_);
            if (!path.empty()) {
                const char* ext_map[] = {"png","jpg","bmp","tga"};
                std::string fmt = ext_map[save_format_idx_];
                if (img_mgr_.SaveImage(path, fmt)) {
                    status_msg_ = "\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98 (Saved): " + path;
                } else {
                    status_msg_ = "\xe4\xbf\x9d\xe5\xad\x98\xe5\xa4\xb1\xe8\xb4\xa5 (Failed to save): " + path;
                }
            }
            show_save_dialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("\xe5\x8f\x96\xe6\xb6\x88 (Cancel)")) show_save_dialog_ = false;
    }
    ImGui::End();
}

void Application::DrawRaw16Dialog() {
    if (!show_raw16_dialog_) return;

    ImGui::SetNextWindowSize(ImVec2(520, 280), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("\xe5\xaf\xbc\xe5\x85\xa5 RAW16 / TIFF (Import RAW16 / TIFF)##dlg",
                     &show_raw16_dialog_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {

        ImGui::Text("\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84 (File path) �?.raw / .bin / .tif / .tiff:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##raw16path", raw16_path_buf_, sizeof(raw16_path_buf_));

        ImGui::Separator();
        ImGui::Text("RAW \xe6\xa0\xbc\xe5\xbc\x8f\xe5\x8f\x82\xe6\x95\xb0 (RAW format parameters):");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("\xe5\xae\xbd (Width)##rw",  &raw16_width_,  0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("\xe9\xab\x98 (Height)##rh", &raw16_height_, 0, 0);
        if (raw16_width_  < 1) raw16_width_  = 1;
        if (raw16_height_ < 1) raw16_height_ = 1;

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("\xe5\xba\x8f\xe5\xa4\xb4\xe5\xad\x97\xe8\x8a\x82 (Header Offset bytes)##roff", &raw16_offset_, 0, 0);
        if (raw16_offset_ < 0) raw16_offset_ = 0;

        ImGui::Checkbox("\xe5\xa4\xa7\xe7\xab\xaf\xe5\xad\x97\xe8\x8a\x82\xe5\xba\x8f (Big-endian)##rbe", &raw16_big_endian_);

        ImGui::Separator();

        // Try to detect format from extension
        std::string path_str(raw16_path_buf_);
        auto ext_pos = path_str.rfind('.');
        bool is_tiff = false;
        if (ext_pos != std::string::npos) {
            std::string ext = path_str.substr(ext_pos);
            for (auto& ch : ext) ch = static_cast<char>(tolower(ch));
            is_tiff = (ext == ".tif" || ext == ".tiff");
        }

        if (is_tiff)
            ImGui::TextDisabled("\xe6\xa3\x80\xe6\xb5\x8b\xe5\x88\xb0 TIFF \xe6\x89\xa9\xe5\xb1\x95\xe5\x90\x8d\xef\xbc\x8c\xe5\xb0\x86\xe4\xbd\xbf\xe7\x94\xa8 TIFF \xe8\xaf\xa5\xe5\x99\xa8\xe8\xaf\xbb\xe5\x8f\x96 (TIFF extension detected �?will use TIFF reader)");
        else
            ImGui::TextDisabled("\xe5\x85\xb6\xe4\xbb\x96\xe6\x89\xa9\xe5\xb1\x95\xe5\x90\x8d\xe5\xb0\x86\xe4\xbd\xbf\xe7\x94\xa8 RAW16 \xe8\xaf\xa5\xe5\x99\xa8\xe8\xaf\xbb\xe5\x8f\x96 (Other extension �?will use RAW16 reader)");

        if (ImGui::Button("\xe5\xaf\xbc\xe5\x85\xa5 (Import)")) {
            std::string path(raw16_path_buf_);
            bool ok = false;
            if (!path.empty()) {
                raw16_.FreeTexture();
                if (is_tiff) {
                    ok = LoadTiff16(path, raw16_);
                    if (!ok) {
                        // Fallback: try stbi_load_16 (handles 16-bit PNG, some others)
                        ok = LoadImage16(path, raw16_);
                    }
                } else {
                    // Try RAW16 first; if it fails try stbi_load_16
                    ok = LoadRaw16(path, raw16_width_, raw16_height_,
                                   raw16_offset_, raw16_big_endian_, raw16_);
                    if (!ok)
                        ok = LoadImage16(path, raw16_);
                }
                if (ok) {
                    zoom_ = 1.f; pan_x_ = pan_y_ = 0.f;
                    mode_16bit_ = true;
                    status_msg_ = "\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd 16-bit (Loaded 16-bit): " + path;
                    glfwSetWindowTitle(window_, ("ImgToolAi [16-bit] - " + path).c_str());
                } else {
                    status_msg_ = "\xe5\xaf\xbc\xe5\x85\xa5\xe5\xa4\xb1\xe8\xb4\xa5 (Import failed): " + path;
                }
            }
            show_raw16_dialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("\xe5\x8f\x96\xe6\xb6\x88 (Cancel)")) show_raw16_dialog_ = false;
    }
    ImGui::End();
}


