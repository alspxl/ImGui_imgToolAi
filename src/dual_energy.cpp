// dual_energy.cpp
// Implementations of dual-energy X-ray rendering algorithms.
//
// Physics background
// ──────────────────
// For a monochromatic beam the Beer–Lambert projection through a pixel column
// is p = ∫ µ(E, x) dx.  In the dual-energy model the total attenuation is
// decomposed into photoelectric (PE) and Compton (C) contributions:
//
//   µ(E) = a_PE · f_PE(E)  +  a_C · f_C(E)
//
// where  f_PE(E) ∝ Z^3.8 / E^3.2  and  f_C(E) ≈ σ_KN(E) · Z/A.
//
// Two energy measurements (p_L, p_H) then give two equations whose solution
// yields the Z-sensitive PE component – the basis of LHZ and LHD.

#include "dual_energy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace DualEnergy {

// ── Internal helpers ──────────────────────────────────────────────────────────

// Small epsilon used throughout to guard against division by zero.
static constexpr float EPSILON = 1e-8f;

// Larger epsilon for checks where absolute scale matters less.
static constexpr float EPSILON_PROJ = 1e-7f;

// Determine the air-reference level for a channel.
// If ref_hint > 0, use it directly; otherwise use the maximum pixel value.
// The returned value is always >= 1.0 to prevent division by zero in ToProj.
static float ComputeRef(const uint16_t* pix, int n, float ref_hint) {
    if (ref_hint > 0.f) return ref_hint < 1.f ? 1.f : ref_hint;
    float mx = 1.f;
    for (int i = 0; i < n; ++i) {
        if (pix[i] > mx) mx = static_cast<float>(pix[i]);
    }
    return mx;
}

// Convert a raw 16-bit detector value to a log-projection value.
//   p = −ln(I / I₀),  clamped so that I/I₀ ∈ [ε, 1].
static inline float ToProj(uint16_t val, float ref) {
    float t = static_cast<float>(val) / ref;
    if (t < EPSILON_PROJ) t = EPSILON_PROJ;
    if (t > 1.f)   t = 1.f;
    return -std::log(t);   // p ∈ [0, +∞)
}

// HSV → RGB conversion (H in [0°, 360°), S/V in [0, 1]).
static void HSVtoRGB(float h, float s, float v,
                     uint8_t& r, uint8_t& g, uint8_t& b) {
    // Clamp S and V.
    s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
    v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);

    if (s < 1e-6f) {
        uint8_t gv = static_cast<uint8_t>(v * 255.f + 0.5f);
        r = g = b = gv;
        return;
    }

    h = std::fmod(h, 360.f);
    if (h < 0.f) h += 360.f;

    float hh = h / 60.f;
    int   i  = static_cast<int>(hh) % 6;
    float ff = hh - static_cast<int>(hh);

    float p = v * (1.f - s);
    float q = v * (1.f - s * ff);
    float t = v * (1.f - s * (1.f - ff));

    float rv, gv, bv;
    switch (i) {
        case 0:  rv = v; gv = t; bv = p; break;
        case 1:  rv = q; gv = v; bv = p; break;
        case 2:  rv = p; gv = v; bv = t; break;
        case 3:  rv = p; gv = q; bv = v; break;
        case 4:  rv = t; gv = p; bv = v; break;
        default: rv = v; gv = p; bv = q; break;
    }

    auto to8 = [](float x) -> uint8_t {
        if (x <= 0.f)  return 0;
        if (x >= 1.f)  return 255;
        return static_cast<uint8_t>(x * 255.f + 0.5f);
    };
    r = to8(rv);
    g = to8(gv);
    b = to8(bv);
}

// Map a computed Z/ratio quantity to an RGBA8 pseudocolor pixel.
//
//   quantity   : the Z-sensitive scalar (ratio, Z_eff, diff, fraction …)
//   q_min/max  : display range mapped to H_low/H_high
//   total_p    : p_L + p_H  (drives brightness via Beer–Lambert)
//   att_scale  : V = exp(−att_scale · total_p)
//   cs         : colorscheme
static void MapColor(float quantity,
                     float q_min, float q_max,
                     float total_p, float att_scale,
                     Colorscheme cs,
                     uint8_t& r, uint8_t& g, uint8_t& b) {
    // Normalise quantity → [0, 1].
    float qt = (quantity - q_min) / (q_max - q_min + EPSILON);
    qt = qt < 0.f ? 0.f : (qt > 1.f ? 1.f : qt);

    // Brightness from total attenuation.
    float V = std::exp(-att_scale * total_p);
    V = V < 0.f ? 0.f : (V > 1.f ? 1.f : V);

    if (cs == Colorscheme::Grayscale) {
        uint8_t grey = static_cast<uint8_t>(qt * V * 255.f + 0.5f);
        r = g = b = grey;
        return;
    }

    // Hue range for the chosen colorscheme.
    float H_low, H_high;
    if (cs == Colorscheme::Security) {
        H_low  = 30.f;   // orange – low-Z, organic
        H_high = 240.f;  // blue   – high-Z, metal
    } else {             // Mining
        H_low  = 30.f;   // orange
        H_high = 200.f;  // cyan-blue
    }

    float H = H_low + qt * (H_high - H_low);
    HSVtoRGB(H, 1.0f, V, r, g, b);
}

// ── Per-algorithm renderers ───────────────────────────────────────────────────

// LHR: Low/High Ratio
//   ratio = p_L / p_H
//   Physically: ratio ≈ µ_L / µ_H, which increases monotonically with Z_eff
//   because the photoelectric cross-section (∝ Z^3.8) contributes more at
//   low energies.  Ratio is thickness-independent for a pure material.
static void RenderLHR(const uint16_t* low, const uint16_t* high, int N,
                      float ref_L, float ref_H,
                      const Params& p, uint8_t* out) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        float pL    = ToProj(low[i],  ref_L);
        float pH    = ToProj(high[i], ref_H);
        float ratio = pL / (pH + EPSILON_PROJ);
        float total = pL + pH;

        uint8_t r, g, b;
        MapColor(ratio, p.disp_min, p.disp_max, total, p.att_scale,
                 p.colorscheme, r, g, b);
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = 255;
    }
}

// LHZ: Z_eff estimation via PE / Compton decomposition
//
//   At low energy:  p_L ≈ a_PE · f_PE_L + a_C · f_C_L
//   At high energy: p_H ≈ a_PE · f_PE_H + a_C · f_C_H
//
//   Eliminate Compton term by choosing k_C = f_C_L / f_C_H:
//     PE_signal = p_L − k_C · p_H   (PE-dominant)
//
//   Normalise by total attenuation to remove thickness dependence:
//     R = PE_signal / (p_L + p_H)   R ∈ [0, 1]
//
//   Map to Z_eff via calibration polynomial:
//     Z_eff = a · R^b + c
static void RenderLHZ(const uint16_t* low, const uint16_t* high, int N,
                      float ref_L, float ref_H,
                      const Params& p, uint8_t* out) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        float pL    = ToProj(low[i],  ref_L);
        float pH    = ToProj(high[i], ref_H);
        float total = pL + pH;

        // PE-sensitive signal (cancel Compton contribution).
        float pe_sig = pL - p.z_kC * pH;
        pe_sig = pe_sig < 0.f ? 0.f : pe_sig;   // negative = below-reference noise

        // Z-sensitive ratio R ∈ [0, 1].
        float R = pe_sig / (total + EPSILON);

        // Calibration polynomial: Z_eff = a · R^b + c.
        float z_eff = p.z_a * std::pow(R + EPSILON, p.z_b) + p.z_c;

        uint8_t r, g, b;
        MapColor(z_eff, p.disp_min, p.disp_max, total, p.att_scale,
                 p.colorscheme, r, g, b);
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = 255;
    }
}

// LHD: Log subtraction (basis-material suppression)
//
//   diff = p_L − k · p_H
//
//   Choosing k = µ_ref_L / µ_ref_H makes the reference material (e.g.
//   quartz gangue, aluminium casing) appear as diff ≈ 0, while materials
//   with higher Z produce diff > 0 and lower-Z materials produce diff < 0.
static void RenderLHD(const uint16_t* low, const uint16_t* high, int N,
                      float ref_L, float ref_H,
                      const Params& p, uint8_t* out) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        float pL   = ToProj(low[i],  ref_L);
        float pH   = ToProj(high[i], ref_H);
        float diff = pL - p.lhd_k * pH;
        float total = pL + pH;

        // Map the signed difference: disp_min/max should straddle 0.
        // Example: disp_min = −1.0, disp_max = +1.0
        uint8_t r, g, b;
        MapColor(diff, p.disp_min, p.disp_max, total, p.att_scale,
                 p.colorscheme, r, g, b);
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = 255;
    }
}

// MaterialDecomp: 2-basis material decomposition
//
//   Model: p = M · a,  where M = [[µ₁L, µ₂L], [µ₁H, µ₂H]]
//   Solve for thicknesses:  a = M⁻¹ · p
//
//     a₁ = ( µ₂H·p_L − µ₂L·p_H) / det   (low-Z  basis equivalent thickness)
//     a₂ = ( µ₁L·p_H − µ₁H·p_L) / det   (high-Z basis equivalent thickness)
//     det = µ₁L·µ₂H − µ₂L·µ₁H
//
//   The high-Z fraction  f = a₂ / (a₁ + a₂)  is used as the colour driver.
static void RenderMatDecomp(const uint16_t* low, const uint16_t* high, int N,
                             float ref_L, float ref_H,
                             const Params& p, uint8_t* out) {
    float det = p.mu1L * p.mu2H - p.mu2L * p.mu1H;
    if (std::fabs(det) < EPSILON) {
        // Degenerate basis pair – fall back to LHR.
        RenderLHR(low, high, N, ref_L, ref_H, p, out);
        return;
    }
    float inv_det = 1.f / det;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        float pL = ToProj(low[i],  ref_L);
        float pH = ToProj(high[i], ref_H);

        float a1 = (p.mu2H * pL - p.mu2L * pH) * inv_det;
        float a2 = (p.mu1L * pH - p.mu1H * pL) * inv_det;

        // Clamp negative thicknesses to zero (noise / model mismatch).
        a1 = a1 < 0.f ? 0.f : a1;
        a2 = a2 < 0.f ? 0.f : a2;

        // High-Z fraction as Z indicator.
        float frac  = a2 / (a1 + a2 + EPSILON);
        float total = pL + pH;

        uint8_t r, g, b;
        MapColor(frac, 0.f, 1.f, total, p.att_scale, p.colorscheme, r, g, b);
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = 255;
    }
}

// ── Public entry point ────────────────────────────────────────────────────────

void Render(const uint16_t* low, const uint16_t* high,
            int width, int height,
            const Params& p,
            uint8_t* out) {
    const int N = width * height;

    float ref_L = ComputeRef(low,  N, p.ref_low);
    float ref_H = ComputeRef(high, N, p.ref_high);

    switch (p.algo) {
        case Algorithm::LHR:
            RenderLHR(low, high, N, ref_L, ref_H, p, out);
            break;
        case Algorithm::LHZ:
            RenderLHZ(low, high, N, ref_L, ref_H, p, out);
            break;
        case Algorithm::LHD:
            RenderLHD(low, high, N, ref_L, ref_H, p, out);
            break;
        case Algorithm::MaterialDecomp:
            RenderMatDecomp(low, high, N, ref_L, ref_H, p, out);
            break;
    }
}

} // namespace DualEnergy
