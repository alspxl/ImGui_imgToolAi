// raw16_state.cpp
// Implementation of Raw16State tone-mapping pipeline and file loaders.

#include "raw16_state.h"
#include "tiff_reader.h"

// stb_image for 16-bit PNG (implementation in stb_impl.cpp)
#include "stb_image.h"

#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#else
  #include <GL/gl.h>
#endif

// Handle missing OpenGL constants on Windows
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <vector>

// ── Colormap helpers ──────────────────────────────────────────────────────────

static void JetColormap(float t, uint8_t& r, uint8_t& g, uint8_t& b) {
    // Standard Jet colormap approximation.
    auto clamp01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
    float rv = clamp01(1.5f - std::abs(4.f * t - 3.f));
    float gv = clamp01(1.5f - std::abs(4.f * t - 2.f));
    float bv = clamp01(1.5f - std::abs(4.f * t - 1.f));
    r = static_cast<uint8_t>(rv * 255.f + 0.5f);
    g = static_cast<uint8_t>(gv * 255.f + 0.5f);
    b = static_cast<uint8_t>(bv * 255.f + 0.5f);
}

static void HotColormap(float t, uint8_t& r, uint8_t& g, uint8_t& b) {
    // Black → Red → Yellow → White
    auto clamp01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
    r = static_cast<uint8_t>(clamp01(t * 3.f)            * 255.f + 0.5f);
    g = static_cast<uint8_t>(clamp01(t * 3.f - 1.f)      * 255.f + 0.5f);
    b = static_cast<uint8_t>(clamp01(t * 3.f - 2.f)      * 255.f + 0.5f);
}

void ApplyColormap(float t, ColormapType cmap,
                   uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (cmap) {
        case ColormapType::Jet:
            JetColormap(t, r, g, b);
            return;
        case ColormapType::Hot:
            HotColormap(t, r, g, b);
            return;
        default: // Gray
            uint8_t v = static_cast<uint8_t>(t * 255.f + 0.5f);
            r = g = b = v;
            return;
    }
}

// ── Window/Level precompute ───────────────────────────────────────────────────

void ComputeWindowLH(const ToneMapParams& p,
                     const uint16_t* pixels, int n,
                     float& wl, float& wh) {
    if (n <= 0 || !pixels) { wl = 0.f; wh = 65535.f; return; }

    switch (p.mode) {
        case ToneMapMode::WindowLevel:
            wl = p.level - p.window * 0.5f;
            wh = p.level + p.window * 0.5f;
            break;

        case ToneMapMode::AutoMinMax: {
            uint16_t mn = pixels[0], mx = pixels[0];
            for (int i = 1; i < n; ++i) {
                if (pixels[i] < mn) mn = pixels[i];
                if (pixels[i] > mx) mx = pixels[i];
            }
            wl = static_cast<float>(mn);
            wh = static_cast<float>(mx);
            break;
        }

        case ToneMapMode::Percentile: {
            // Build histogram over 16-bit range.
            // Use 1024-bin approximation for speed.
            constexpr int BINS = 1024;
            std::vector<int> hist(BINS, 0);
            for (int i = 0; i < n; ++i)
                ++hist[pixels[i] >> 6]; // 65536/1024 = 64, so >>6
            int total = n;
            float plo = std::max(0.f, std::min(100.f, p.p_low))  * 0.01f;
            float phi = std::max(0.f, std::min(100.f, p.p_high)) * 0.01f;
            int lo_cnt = static_cast<int>(total * plo);
            int hi_cnt = static_cast<int>(total * phi);
            int cum = 0;
            int lo_bin = 0, hi_bin = BINS - 1;
            for (int i = 0; i < BINS; ++i) {
                cum += hist[i];
                if (cum >= lo_cnt && lo_bin == 0) lo_bin = i;
                if (cum <= hi_cnt) hi_bin = i;
            }
            wl = static_cast<float>(lo_bin * 64);
            wh = static_cast<float>(hi_bin * 64 + 63);
            break;
        }
    }

    // Avoid division by zero.
    if (wh <= wl) wh = wl + 1.f;
}

// ── Per-pixel mapping ─────────────────────────────────────────────────────────

void MapPixel(uint16_t val, const ToneMapParams& p,
              float wl, float wh,
              uint8_t& r, uint8_t& g, uint8_t& b) {
    // Linear normalise to [0, 1]
    float t = (static_cast<float>(val) - wl) / (wh - wl);
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);

    // Gamma
    if (p.gamma != 1.0f && p.gamma > 0.f)
        t = std::pow(t, 1.0f / p.gamma);

    // Invert
    if (p.invert) t = 1.0f - t;

    ApplyColormap(t, p.colormap, r, g, b);
}

// ── RGBA8 buffer generation ───────────────────────────────────────────────────

// Fill dst (width*height*4 RGBA8) from src (width*height uint16).
static void FillRGBA(const uint16_t* src, int count,
                     const ToneMapParams& p,
                     uint8_t* dst) {
    float wl, wh;
    ComputeWindowLH(p, src, count, wl, wh);
    for (int i = 0; i < count; ++i) {
        uint8_t r, g, b;
        MapPixel(src[i], p, wl, wh, r, g, b);
        dst[i * 4 + 0] = r;
        dst[i * 4 + 1] = g;
        dst[i * 4 + 2] = b;
        dst[i * 4 + 3] = 255;
    }
}

// ── Raw16State methods ────────────────────────────────────────────────────────

void Raw16State::FreeTexture() {
    if (tex_display) {
        glDeleteTextures(1, &tex_display);
        tex_display = 0;
    }
}

void Raw16State::MapAndUpload() {
    if (!has_data()) return;

    const int W = width;
    const int H = height;
    const int total = W * H;

    // Build RGBA8 display buffer.
    std::vector<uint8_t> rgba(static_cast<size_t>(W) * H * 4);

    switch (layout) {
        case LayoutMode::Single: {
            if (!params_left.dirty && tex_display != 0) return;
            FillRGBA(data.data(), total, params_left, rgba.data());
            params_left.dirty = false;
            break;
        }

        case LayoutMode::Mosaic: {
            if (!params_left.dirty && tex_display != 0) return;
            FillRGBA(data.data(), total, params_left, rgba.data());
            params_left.dirty = false;

            // Optional divider line at the horizontal centre.
            if (show_divider) {
                int cx = W / 2;
                for (int y = 0; y < H; ++y) {
                    int idx = (y * W + cx) * 4;
                    rgba[idx + 0] = 255;
                    rgba[idx + 1] = 255;
                    rgba[idx + 2] = 0;
                    rgba[idx + 3] = 255;
                }
            }
            break;
        }

        case LayoutMode::TwoChannel: {
            bool left_dirty  = params_left.dirty;
            bool right_dirty = link_params ? left_dirty : params_right.dirty;
            if (!left_dirty && !right_dirty && tex_display != 0) return;

            // Width of each half-side in the stored buffer.
            // The stored buffer is W pixels wide; left = first W/2 cols, right = last W/2 cols.
            int half = W / 2;
            if (half <= 0) half = W;

            // Temporary half-width RGBA buffers.
            std::vector<uint8_t> left_rgba(static_cast<size_t>(half) * H * 4);
            std::vector<uint8_t> right_rgba(static_cast<size_t>(half) * H * 4);

            // Extract left pixels (strided).
            std::vector<uint16_t> left_pix(static_cast<size_t>(half) * H);
            std::vector<uint16_t> right_pix(static_cast<size_t>(half) * H);
            for (int y = 0; y < H; ++y) {
                std::memcpy(left_pix.data()  + y * half, data.data() + y * W,          half * sizeof(uint16_t));
                std::memcpy(right_pix.data() + y * half, data.data() + y * W + half,   half * sizeof(uint16_t));
            }

            const ToneMapParams& rp = link_params ? params_left : params_right;
            FillRGBA(left_pix.data(),  half * H, params_left, left_rgba.data());
            FillRGBA(right_pix.data(), half * H, rp,          right_rgba.data());

            params_left.dirty  = false;
            params_right.dirty = false;

            // Stitch into the full-width RGBA buffer.
            for (int y = 0; y < H; ++y) {
                std::memcpy(rgba.data() + y * W * 4,
                            left_rgba.data() + y * half * 4,
                            static_cast<size_t>(half) * 4);
                std::memcpy(rgba.data() + y * W * 4 + half * 4,
                            right_rgba.data() + y * half * 4,
                            static_cast<size_t>(half) * 4);
            }

            // Optional divider line.
            if (show_divider) {
                for (int y = 0; y < H; ++y) {
                    int idx = (y * W + half) * 4;
                    rgba[idx + 0] = 255;
                    rgba[idx + 1] = 255;
                    rgba[idx + 2] = 0;
                    rgba[idx + 3] = 255;
                }
            }
            break;
        }

        case LayoutMode::DualAligned: {
            bool left_dirty  = params_left.dirty;
            bool right_dirty = link_params ? left_dirty : params_right.dirty;
            
            int half = W / 2;
            if (half <= 0) half = W;

            std::vector<uint8_t> left_rgba(static_cast<size_t>(half) * H * 4);
            std::vector<uint8_t> right_rgba(static_cast<size_t>(half) * H * 4);

            std::vector<uint16_t> left_pix(static_cast<size_t>(half) * H);
            std::vector<uint16_t> right_pix(static_cast<size_t>(half) * H);
            
            for (int y = 0; y < H; ++y) {
                std::memcpy(left_pix.data()  + y * half, data.data() + y * W,          half * sizeof(uint16_t));
                std::memcpy(right_pix.data() + y * half, data.data() + y * W + half,   half * sizeof(uint16_t));
            }

            const ToneMapParams& rp = link_params ? params_left : params_right;
            FillRGBA(left_pix.data(),  half * H, params_left, left_rgba.data());
            FillRGBA(right_pix.data(), half * H, rp,          right_rgba.data());

            params_left.dirty  = false;
            params_right.dirty = false;

            // Composite Low and High (Left: Blue, Right: Green) with offset applied to Right
            // Only using half width for display. We need to resize rgba vector so it's W/2 x H.
            rgba.resize(static_cast<size_t>(half) * H * 4);
            
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < half; ++x) {
                    size_t out_idx = (y * half + x) * 4;
                    // Left image goes to Blue
                    rgba[out_idx + 0] = 0;                      // R
                    rgba[out_idx + 1] = 0;                      // G
                    rgba[out_idx + 2] = left_rgba[out_idx + 2]; // B
                    rgba[out_idx + 3] = 255;
                    
                    // Right image goes to Green, with offset
                    int shift_x = x - align_offset_x;
                    int shift_y = y - align_offset_y;
                    if (shift_x >= 0 && shift_x < half && shift_y >= 0 && shift_y < H) {
                        size_t in_idx = (shift_y * half + shift_x) * 4;
                        rgba[out_idx + 1] = right_rgba[in_idx + 1]; // G
                    }
                }
            }
            break;
        }
    }

    int out_w = width;
    if (layout == LayoutMode::DualAligned) {
        out_w = width / 2;
    }

    // Upload RGBA8 to OpenGL texture.
    if (tex_display == 0)
        glGenTextures(1, &tex_display);

    glBindTexture(GL_TEXTURE_2D, tex_display);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_w, H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ── File loaders ──────────────────────────────────────────────────────────────

#include <iostream>

#ifdef _WIN32
#include <windows.h>
static std::wstring Utf8ToWstr(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
#endif

bool LoadRaw16(const std::string& path,
               int width, int height, int offset,
               bool big_endian,
               Raw16State& out) {
    if (width <= 0 || height <= 0) {
        std::cerr << "[LoadRaw16] Error: Invalid dimensions (" << width << "x" << height << ")\n";
        return false;
    }

#ifdef _WIN32
    std::ifstream f(Utf8ToWstr(path).c_str(), std::ios::binary);
#else
    std::ifstream f(path, std::ios::binary);
#endif
    if (!f.is_open()) {
        std::cerr << "[LoadRaw16] Error: Failed to open file: " << path << "\n";
        return false;
    }

    // Validate file size.
    f.seekg(0, std::ios::end);
    std::streamoff file_size = f.tellg();
    std::streamoff needed = static_cast<std::streamoff>(offset) +
                            static_cast<std::streamoff>(width) *
                            static_cast<std::streamoff>(height) *
                            static_cast<std::streamoff>(sizeof(uint16_t));
    if (file_size < needed) {
        std::cerr << "[LoadRaw16] Error: File too small! Size is " << file_size 
                  << ", but needed " << needed << " bytes (W=" << width 
                  << " H=" << height << " Offset=" << offset << ").\n";
        return false;
    }

    f.seekg(offset, std::ios::beg);

    size_t npix = static_cast<size_t>(width) * height;
    std::vector<uint16_t> buf(npix);
    f.read(reinterpret_cast<char*>(buf.data()),
           static_cast<std::streamsize>(npix * sizeof(uint16_t)));
    if (f.gcount() != static_cast<std::streamsize>(npix * sizeof(uint16_t))) {
        std::cerr << "[LoadRaw16] Error: Read failed, read " << f.gcount() 
                  << " bytes, expected " << (npix * sizeof(uint16_t)) << "\n";
        return false;
    }

    if (big_endian) {
        for (auto& v : buf) {
            v = static_cast<uint16_t>((v >> 8) | (v << 8));
        }
    }

    out.data   = std::move(buf);
    out.width  = width;
    out.height = height;
    out.MarkDirty();
    return true;
}

bool LoadImage16(const std::string& path, Raw16State& out) {
    int w = 0, h = 0, c = 0;
#ifdef _WIN32
    FILE* f = _wfopen(Utf8ToWstr(path).c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    std::vector<uint8_t> buffer(size);
    fread(buffer.data(), 1, size, f);
    fclose(f);

    uint16_t* raw = stbi_load_16_from_memory(buffer.data(), size, &w, &h, &c, 1);
    if (!raw) return false;

    size_t npix = static_cast<size_t>(w) * h;
    out.data.assign(raw, raw + npix);
    stbi_image_free(raw);

    out.width  = w;
    out.height = h;
    out.MarkDirty();
    return true;
}

bool LoadTiff16(const std::string& path, Raw16State& out) {
    int w = 0, h = 0;
    std::vector<uint16_t> buf;
    if (!TiffReader::Load16(path, w, h, buf)) return false;

    out.data   = std::move(buf);
    out.width  = w;
    out.height = h;
    out.MarkDirty();
    return true;
}
