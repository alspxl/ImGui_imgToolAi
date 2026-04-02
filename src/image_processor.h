#pragma once
// image_processor.h
// Pure C++ image processing algorithms that operate directly on pixel buffers.
// All functions expect RGBA (4-channel) pixel data unless noted.

#include <cstdint>

namespace ImageProcessor {

// ── Basic adjustments ────────────────────────────────────────────────────────

// Brightness: add offset to each R/G/B channel, clamped to [0,255].
void AdjustBrightness(unsigned char* pixels, int width, int height, int channels, int offset);

// Contrast: scale each R/G/B channel around 128.
// factor = 1.0 means no change; >1 increases contrast; <1 decreases it.
void AdjustContrast(unsigned char* pixels, int width, int height, int channels, float factor);

// Gamma correction: newVal = 255 * pow(oldVal/255, 1/gamma).
void AdjustGamma(unsigned char* pixels, int width, int height, int channels, float gamma);

// ── Colour adjustments ───────────────────────────────────────────────────────

// HSL adjustment. hueShift in degrees [-180,180], satScale and lightShift are additive.
void AdjustHSL(unsigned char* pixels, int width, int height, int channels,
               float hueShift, float satScale, float lightShift);

// Per-channel gain adjustment. rGain/gGain/bGain around 1.0.
void AdjustColorBalance(unsigned char* pixels, int width, int height, int channels,
                        float rGain, float gGain, float bGain);

// Convert to greyscale (result stored back in the same buffer with original channel count).
void Grayscale(unsigned char* pixels, int width, int height, int channels);

// Invert each R/G/B channel.
void Invert(unsigned char* pixels, int width, int height, int channels);

// Posterize: quantize each channel to `levels` evenly-spaced values.
void Posterize(unsigned char* pixels, int width, int height, int channels, int levels);

// ── Filter effects ───────────────────────────────────────────────────────────

// Gaussian blur using separable two-pass convolution; radius in [1,20].
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* GaussianBlur(const unsigned char* pixels, int width, int height, int channels, int radius);

// 3×3 unsharp sharpen.
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* Sharpen(const unsigned char* pixels, int width, int height, int channels);

// 3×3 emboss kernel.
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* Emboss(const unsigned char* pixels, int width, int height, int channels);

// Sobel edge detection (result is greyscale encoded into all channels).
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* EdgeDetect(const unsigned char* pixels, int width, int height, int channels);

// Simplified dark-channel-prior dehaze.
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* Dehaze(const unsigned char* pixels, int width, int height, int channels);

// ── Geometric transforms ─────────────────────────────────────────────────────

// Bilinear-interpolation resize.
// Returns a newly allocated buffer; out_width/out_height set on return (caller must delete[]).
unsigned char* Resize(const unsigned char* pixels, int width, int height, int channels,
                      int new_width, int new_height);

// 90°/180°/270° rotation. degrees must be 90, 180, or 270.
// Returns a newly allocated buffer; out_width/out_height are set appropriately (caller must delete[]).
unsigned char* Rotate(const unsigned char* pixels, int width, int height, int channels,
                      int degrees, int& out_width, int& out_height);

// Flip horizontally (mirror left-right).
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* FlipHorizontal(const unsigned char* pixels, int width, int height, int channels);

// Flip vertically (mirror top-bottom).
// Returns a newly allocated buffer (caller must delete[]).
unsigned char* FlipVertical(const unsigned char* pixels, int width, int height, int channels);

} // namespace ImageProcessor
