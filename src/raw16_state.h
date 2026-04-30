#pragma once
// raw16_state.h
// 16-bit grayscale image state: data storage, layout modes, tone-mapping params,
// and cached RGBA8 display textures.  Also owns the dual-energy render output.

#include "dual_energy.h"

#include <cstdint>
#include <string>
#include <vector>

typedef unsigned int GLuint;

// ── Enums ─────────────────────────────────────────────────────────────────────

// How the pixel buffer is interpreted and displayed.
enum class LayoutMode {
    Single,      // Normal single-view display (full buffer, one set of params).
    TwoChannel,  // Left half / right half are two independent detector channels.
    Mosaic,      // Left and right halves are a spliced mosaic of one channel.
    DualAligned  // Left and right halves overlaid (e.g. Cyan/Red) with offset.
};

// Tone-mapping mode for 16-bit → 8-bit conversion.
enum class ToneMapMode {
    AutoMinMax,  // Stretch full [min, max] range to [0, 255].
    Percentile,  // Stretch [p_low%, p_high%] to [0, 255].
    WindowLevel  // Manual window/level (CT-style).
};

// Colormap applied after 16-bit → normalised [0,1] mapping.
enum class ColormapType {
    Gray = 0,
    Jet,
    Hot
};

// ── ToneMapParams ─────────────────────────────────────────────────────────────

struct ToneMapParams {
    ToneMapMode  mode      = ToneMapMode::AutoMinMax;
    float        level     = 32767.f; // centre of display window (WindowLevel mode)
    float        window    = 65535.f; // width of display window  (WindowLevel mode)
    float        gamma     = 1.0f;    // gamma correction (applied after linear map)
    bool         invert    = false;   // invert output
    ColormapType colormap  = ColormapType::Gray;
    float        p_low     = 1.0f;    // lower percentile (Percentile mode)
    float        p_high    = 99.0f;   // upper percentile (Percentile mode)

    // Marks whether the cached display buffer needs recomputing.
    bool         dirty     = true;
};

// ── Raw16State ────────────────────────────────────────────────────────────────

struct Raw16State {
    // ── Raw pixel data ──────────────────────────────────────────────────────
    std::vector<uint16_t> data;   // full buffer (width × height uint16 values)
    int width  = 0;               // logical width  (half of stored width for side-by-side)
    int height = 0;

    // ── Layout & display config ─────────────────────────────────────────────
    LayoutMode layout      = LayoutMode::Single;
    bool       link_params = true;   // Two-Channel: link left/right params
    bool       show_divider = true;  // Mosaic / Two-Channel: draw centre divider
    int        align_offset_x = 0;   // DualAligned: X offset
    int        align_offset_y = 0;   // DualAligned: Y offset

    // ── Tone-map params (left = shared in Single/Mosaic; right = Two-Channel only) ──
    ToneMapParams params_left;
    ToneMapParams params_right;

    // ── Dual-energy rendering ───────────────────────────────────────────────
    // Active when show_dual == true and layout == LayoutMode::TwoChannel.
    // The left half of data[] is the low-energy channel; the right half is
    // the high-energy channel.
    DualEnergy::Params de_params;
    bool               show_dual  = false;  // show dual-energy output in viewer
    GLuint             tex_dual   = 0;      // RGBA8 texture of the DE result

    // ── OpenGL textures ─────────────────────────────────────────────────────
    // In Single / Mosaic mode only tex_display is used.
    // In Two-Channel mode we upload one combined texture (L on left, R on right).
    GLuint tex_display = 0;

    // ── Query helpers ───────────────────────────────────────────────────────
    bool has_data()  const { return !data.empty() && width > 0 && height > 0; }

    // Logical display width (half of data width for side-by-side layouts).
    int display_width() const {
        if (layout == LayoutMode::Single)
            return width;
        return width / 2;
    }

    // Release OpenGL texture.
    void FreeTexture();

    // Recompute RGBA8 and upload to tex_display.  Respects dirty flags.
    void MapAndUpload();

    // Mark both param sets as dirty (force recompute on next MapAndUpload).
    void MarkDirty() {
        params_left.dirty  = true;
        params_right.dirty = true;
        de_params.dirty    = true;
    }

    // Compute the dual-energy RGBA8 result and upload to tex_dual.
    // Requires layout == LayoutMode::TwoChannel.
    // No-op if de_params.dirty == false and tex_dual is already valid.
    void RenderDualEnergy();
};

// ── Stand-alone helper functions ──────────────────────────────────────────────

// Load a RAW 16-bit file into state.data.
// offset: bytes to skip at file start.
// big_endian: if true, swap byte order after reading.
// width/height: logical image dimensions; total pixels read = width * height.
bool LoadRaw16(const std::string& path,
               int width, int height, int offset,
               bool big_endian,
               Raw16State& out);

// Load a 16-bit image using stb_image_16 (handles 16-bit PNG, etc.).
// Returns false if the file is not a recognised format or not 16-bit.
bool LoadImage16(const std::string& path, Raw16State& out);

// Load a 16-bit TIFF using the built-in minimal TIFF reader.
bool LoadTiff16(const std::string& path, Raw16State& out);

// Apply a colormap to t∈[0,1] → (r, g, b) each in [0, 255].
void ApplyColormap(float t, ColormapType cmap,
                   uint8_t& r, uint8_t& g, uint8_t& b);

// Map a single uint16 value through ToneMapParams.
// wl / wh: pre-computed window low/high (call once per frame, not per pixel).
void MapPixel(uint16_t val, const ToneMapParams& p,
              float wl, float wh,
              uint8_t& r, uint8_t& g, uint8_t& b);

// Compute wl/wh for a given ToneMapParams and a pixel block.
void ComputeWindowLH(const ToneMapParams& p,
                     const uint16_t* pixels, int n,
                     float& wl, float& wh);
