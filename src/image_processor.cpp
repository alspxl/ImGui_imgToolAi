// image_processor.cpp
// Pure C++ implementations of all image processing algorithms.

#include "image_processor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ImageProcessor {

// ── Helpers ──────────────────────────────────────────────────────────────────

static inline unsigned char Clamp8(float v) {
    if (v <= 0.f) return 0;
    if (v >= 255.f) return 255;
    return static_cast<unsigned char>(v + 0.5f);
}

static inline unsigned char Clamp8(int v) {
    if (v <= 0) return 0;
    if (v >= 255) return 255;
    return static_cast<unsigned char>(v);
}

// RGB <-> HSL conversions (H in [0,360), S/L in [0,1]).
static void RGBtoHSL(float r, float g, float b, float& h, float& s, float& l) {
    r /= 255.f; g /= 255.f; b /= 255.f;
    float mx = std::max({r, g, b});
    float mn = std::min({r, g, b});
    l = (mx + mn) * 0.5f;
    float delta = mx - mn;
    if (delta < 1e-6f) { h = s = 0.f; return; }
    s = (l > 0.5f) ? delta / (2.f - mx - mn) : delta / (mx + mn);
    if      (mx == r) h = (g - b) / delta + (g < b ? 6.f : 0.f);
    else if (mx == g) h = (b - r) / delta + 2.f;
    else              h = (r - g) / delta + 4.f;
    h *= 60.f;
}

static float HueToRGB(float p, float q, float t) {
    if (t < 0.f) t += 1.f;
    if (t > 1.f) t -= 1.f;
    if (t < 1.f/6.f) return p + (q - p) * 6.f * t;
    if (t < 0.5f)    return q;
    if (t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
    return p;
}

static void HSLtoRGB(float h, float s, float l, float& r, float& g, float& b) {
    if (s < 1e-6f) { r = g = b = l * 255.f; return; }
    float q = (l < 0.5f) ? l * (1.f + s) : l + s - l * s;
    float p = 2.f * l - q;
    float hn = h / 360.f;
    r = HueToRGB(p, q, hn + 1.f/3.f) * 255.f;
    g = HueToRGB(p, q, hn)           * 255.f;
    b = HueToRGB(p, q, hn - 1.f/3.f) * 255.f;
}

// ── Basic adjustments ─────────────────────────────────────────────────────────

void AdjustBrightness(unsigned char* pixels, int width, int height, int channels, int offset) {
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        int nch = std::min(channels, 3);
        for (int c = 0; c < nch; ++c)
            px[c] = Clamp8(static_cast<int>(px[c]) + offset);
    }
}

void AdjustContrast(unsigned char* pixels, int width, int height, int channels, float factor) {
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        int nch = std::min(channels, 3);
        for (int c = 0; c < nch; ++c)
            px[c] = Clamp8((static_cast<float>(px[c]) - 128.f) * factor + 128.f);
    }
}

void AdjustGamma(unsigned char* pixels, int width, int height, int channels, float gamma) {
    if (gamma <= 0.f) gamma = 0.001f;
    float inv = 1.f / gamma;
    // Build LUT for speed.
    unsigned char lut[256];
    for (int i = 0; i < 256; ++i)
        lut[i] = Clamp8(255.f * std::pow(i / 255.f, inv));

    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        int nch = std::min(channels, 3);
        for (int c = 0; c < nch; ++c)
            px[c] = lut[px[c]];
    }
}

// ── Colour adjustments ────────────────────────────────────────────────────────

void AdjustHSL(unsigned char* pixels, int width, int height, int channels,
               float hueShift, float satScale, float lightShift) {
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        float h, s, l;
        RGBtoHSL(px[0], px[1], px[2], h, s, l);
        h = std::fmod(h + hueShift + 360.f, 360.f);
        s = std::clamp(s * (1.f + satScale), 0.f, 1.f);
        l = std::clamp(l + lightShift, 0.f, 1.f);
        float r, g, b;
        HSLtoRGB(h, s, l, r, g, b);
        px[0] = Clamp8(r);
        px[1] = Clamp8(g);
        px[2] = Clamp8(b);
    }
}

void AdjustColorBalance(unsigned char* pixels, int width, int height, int channels,
                        float rGain, float gGain, float bGain) {
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        px[0] = Clamp8(px[0] * rGain);
        px[1] = Clamp8(px[1] * gGain);
        px[2] = Clamp8(px[2] * bGain);
    }
}

void Grayscale(unsigned char* pixels, int width, int height, int channels) {
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        float grey = 0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2];
        unsigned char g = Clamp8(grey);
        px[0] = px[1] = px[2] = g;
    }
}

void Invert(unsigned char* pixels, int width, int height, int channels) {
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        int nch = std::min(channels, 3);
        for (int c = 0; c < nch; ++c)
            px[c] = static_cast<unsigned char>(255 - px[c]);
    }
}

void Posterize(unsigned char* pixels, int width, int height, int channels, int levels) {
    if (levels < 2) levels = 2;
    if (levels > 255) levels = 255;
    // Build LUT
    unsigned char lut[256];
    float step = 255.f / (levels - 1);
    for (int i = 0; i < 256; ++i) {
        int bucket = static_cast<int>(std::round(i / step));
        lut[i] = Clamp8(static_cast<float>(bucket) * step);
    }
    int total = width * height;
    for (int i = 0; i < total; ++i) {
        unsigned char* px = pixels + i * channels;
        int nch = std::min(channels, 3);
        for (int c = 0; c < nch; ++c)
            px[c] = lut[px[c]];
    }
}

// ── Filter effects ────────────────────────────────────────────────────────────

unsigned char* GaussianBlur(const unsigned char* pixels, int width, int height, int channels, int radius) {
    if (radius < 1) radius = 1;
    if (radius > 20) radius = 20;

    // Build 1-D Gaussian kernel.
    int ksize = radius * 2 + 1;
    std::vector<float> kernel(ksize);
    float sigma = radius * 0.5f;
    float sum = 0.f;
    for (int i = 0; i < ksize; ++i) {
        float x = static_cast<float>(i - radius);
        kernel[i] = std::exp(-(x * x) / (2.f * sigma * sigma));
        sum += kernel[i];
    }
    for (auto& v : kernel) v /= sum;

    std::size_t total = static_cast<std::size_t>(width) * height * channels;
    auto* tmp = new unsigned char[total];
    auto* out = new unsigned char[total];

    // Horizontal pass.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                if (c == 3 && channels == 4) {
                    // Preserve alpha.
                    tmp[(y * width + x) * channels + c] = pixels[(y * width + x) * channels + c];
                    continue;
                }
                float acc = 0.f;
                for (int k = -radius; k <= radius; ++k) {
                    int sx = std::clamp(x + k, 0, width - 1);
                    acc += pixels[(y * width + sx) * channels + c] * kernel[k + radius];
                }
                tmp[(y * width + x) * channels + c] = Clamp8(acc);
            }
        }
    }
    // Vertical pass.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                if (c == 3 && channels == 4) {
                    out[(y * width + x) * channels + c] = tmp[(y * width + x) * channels + c];
                    continue;
                }
                float acc = 0.f;
                for (int k = -radius; k <= radius; ++k) {
                    int sy = std::clamp(y + k, 0, height - 1);
                    acc += tmp[(sy * width + x) * channels + c] * kernel[k + radius];
                }
                out[(y * width + x) * channels + c] = Clamp8(acc);
            }
        }
    }
    delete[] tmp;
    return out;
}

unsigned char* Sharpen(const unsigned char* pixels, int width, int height, int channels) {
    // 3×3 sharpen kernel:  [ 0, -1,  0 ]
    //                      [-1,  5, -1 ]
    //                      [ 0, -1,  0 ]
    static const float kern[3][3] = {{0,-1,0},{-1,5,-1},{0,-1,0}};
    std::size_t total = static_cast<std::size_t>(width) * height * channels;
    auto* out = new unsigned char[total];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                if (c == 3 && channels == 4) {
                    out[(y * width + x) * channels + c] = pixels[(y * width + x) * channels + c];
                    continue;
                }
                float acc = 0.f;
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        int sy = std::clamp(y + ky, 0, height - 1);
                        int sx = std::clamp(x + kx, 0, width  - 1);
                        acc += pixels[(sy * width + sx) * channels + c] * kern[ky+1][kx+1];
                    }
                }
                out[(y * width + x) * channels + c] = Clamp8(acc);
            }
        }
    }
    return out;
}

unsigned char* Emboss(const unsigned char* pixels, int width, int height, int channels) {
    // Emboss kernel: [-2,-1,0],[-1,1,1],[0,1,2]
    static const float kern[3][3] = {{-2,-1,0},{-1,1,1},{0,1,2}};
    std::size_t total = static_cast<std::size_t>(width) * height * channels;
    auto* out = new unsigned char[total];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                if (c == 3 && channels == 4) {
                    out[(y * width + x) * channels + c] = pixels[(y * width + x) * channels + c];
                    continue;
                }
                float acc = 0.f;
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        int sy = std::clamp(y + ky, 0, height - 1);
                        int sx = std::clamp(x + kx, 0, width  - 1);
                        acc += pixels[(sy * width + sx) * channels + c] * kern[ky+1][kx+1];
                    }
                }
                out[(y * width + x) * channels + c] = Clamp8(acc + 128.f);
            }
        }
    }
    return out;
}

unsigned char* EdgeDetect(const unsigned char* pixels, int width, int height, int channels) {
    // Sobel operator.
    static const float kx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    static const float ky[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    std::size_t total = static_cast<std::size_t>(width) * height * channels;
    auto* out = new unsigned char[total];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Compute gradient magnitude on luminance.
            float gx = 0.f, gy = 0.f;
            for (int ky_ = -1; ky_ <= 1; ++ky_) {
                for (int kx_ = -1; kx_ <= 1; ++kx_) {
                    int sy = std::clamp(y + ky_, 0, height - 1);
                    int sx = std::clamp(x + kx_, 0, width  - 1);
                    const unsigned char* p = pixels + (sy * width + sx) * channels;
                    float lum = 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
                    gx += lum * kx[ky_+1][kx_+1];
                    gy += lum * ky[ky_+1][kx_+1];
                }
            }
            float mag = std::min(255.f, std::sqrt(gx*gx + gy*gy));
            unsigned char v = Clamp8(mag);
            unsigned char* op = out + (y * width + x) * channels;
            op[0] = op[1] = op[2] = v;
            if (channels == 4) op[3] = 255;
        }
    }
    return out;
}

unsigned char* Dehaze(const unsigned char* pixels, int width, int height, int channels) {
    // Simplified dark-channel prior dehaze.
    // Step 1: compute dark channel with a small patch.
    int patch = std::max(3, std::min(width, height) / 30);
    std::vector<float> dark(static_cast<std::size_t>(width) * height, 255.f);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float mn = 255.f;
            for (int py = -patch; py <= patch; ++py) {
                for (int px = -patch; px <= patch; ++px) {
                    int sy = std::clamp(y + py, 0, height - 1);
                    int sx = std::clamp(x + px, 0, width  - 1);
                    const unsigned char* p = pixels + (sy * width + sx) * channels;
                    float minc = std::min({(float)p[0], (float)p[1], (float)p[2]});
                    mn = std::min(mn, minc);
                }
            }
            dark[y * width + x] = mn;
        }
    }

    // Step 2: estimate atmospheric light as top-0.1% brightest dark-channel pixels.
    float atm = 0.f;
    {
        std::vector<float> sorted_dark = dark;
        std::sort(sorted_dark.begin(), sorted_dark.end());
        std::size_t idx = static_cast<std::size_t>(sorted_dark.size() * 0.999f);
        atm = sorted_dark[idx];
    }
    if (atm < 1.f) atm = 1.f;

    // Step 3: estimate transmission and recover scene radiance.
    float omega = 0.95f; // haze removal factor
    std::size_t total = static_cast<std::size_t>(width) * height * channels;
    auto* out = new unsigned char[total];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float t = 1.f - omega * (dark[y * width + x] / atm);
            t = std::max(t, 0.1f); // clamp minimum transmission

            const unsigned char* src = pixels + (y * width + x) * channels;
            unsigned char*       dst = out     + (y * width + x) * channels;
            for (int c = 0; c < std::min(channels, 3); ++c) {
                float recovered = (src[c] - atm) / t + atm;
                dst[c] = Clamp8(recovered);
            }
            if (channels == 4) dst[3] = src[3];
        }
    }
    return out;
}

// ── Geometric transforms ──────────────────────────────────────────────────────

unsigned char* Resize(const unsigned char* pixels, int width, int height, int channels,
                      int new_width, int new_height) {
    auto* out = new unsigned char[static_cast<std::size_t>(new_width) * new_height * channels];
    float sx = static_cast<float>(width)  / new_width;
    float sy = static_cast<float>(height) / new_height;

    for (int ny = 0; ny < new_height; ++ny) {
        for (int nx = 0; nx < new_width; ++nx) {
            float src_x = (nx + 0.5f) * sx - 0.5f;
            float src_y = (ny + 0.5f) * sy - 0.5f;
            int x0 = std::clamp(static_cast<int>(src_x),     0, width  - 1);
            int x1 = std::clamp(static_cast<int>(src_x) + 1, 0, width  - 1);
            int y0 = std::clamp(static_cast<int>(src_y),     0, height - 1);
            int y1 = std::clamp(static_cast<int>(src_y) + 1, 0, height - 1);
            float fx = src_x - std::floor(src_x);
            float fy = src_y - std::floor(src_y);

            for (int c = 0; c < channels; ++c) {
                float v00 = pixels[(y0 * width + x0) * channels + c];
                float v10 = pixels[(y0 * width + x1) * channels + c];
                float v01 = pixels[(y1 * width + x0) * channels + c];
                float v11 = pixels[(y1 * width + x1) * channels + c];
                float v = v00*(1-fx)*(1-fy) + v10*fx*(1-fy) + v01*(1-fx)*fy + v11*fx*fy;
                out[(ny * new_width + nx) * channels + c] = Clamp8(v);
            }
        }
    }
    return out;
}

unsigned char* Rotate(const unsigned char* pixels, int width, int height, int channels,
                      int degrees, int& out_width, int& out_height) {
    if (degrees == 180) {
        out_width  = width;
        out_height = height;
        auto* out = new unsigned char[static_cast<std::size_t>(width) * height * channels];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int ny = height - 1 - y;
                int nx = width  - 1 - x;
                std::memcpy(out + (ny * width + nx) * channels,
                            pixels + (y * width + x) * channels,
                            channels);
            }
        }
        return out;
    } else if (degrees == 90) {
        // 90° clockwise: (x,y) -> (height-1-y, x)
        out_width  = height;
        out_height = width;
        auto* out = new unsigned char[static_cast<std::size_t>(out_width) * out_height * channels];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int ny = x;
                int nx = height - 1 - y;
                std::memcpy(out + (ny * out_width + nx) * channels,
                            pixels + (y * width + x) * channels,
                            channels);
            }
        }
        return out;
    } else { // 270°
        // 270° clockwise: (x,y) -> (y, width-1-x)
        out_width  = height;
        out_height = width;
        auto* out = new unsigned char[static_cast<std::size_t>(out_width) * out_height * channels];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int ny = width - 1 - x;
                int nx = y;
                std::memcpy(out + (ny * out_width + nx) * channels,
                            pixels + (y * width + x) * channels,
                            channels);
            }
        }
        return out;
    }
}

unsigned char* FlipHorizontal(const unsigned char* pixels, int width, int height, int channels) {
    auto* out = new unsigned char[static_cast<std::size_t>(width) * height * channels];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int nx = width - 1 - x;
            std::memcpy(out + (y * width + nx) * channels,
                        pixels + (y * width + x) * channels,
                        channels);
        }
    }
    return out;
}

unsigned char* FlipVertical(const unsigned char* pixels, int width, int height, int channels) {
    auto* out = new unsigned char[static_cast<std::size_t>(width) * height * channels];
    for (int y = 0; y < height; ++y) {
        int ny = height - 1 - y;
        std::memcpy(out + (ny * width) * channels,
                    pixels + (y  * width) * channels,
                    static_cast<std::size_t>(width) * channels);
    }
    return out;
}

} // namespace ImageProcessor
