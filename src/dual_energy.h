#pragma once
// dual_energy.h
// Dual-energy X-ray rendering algorithms: LHR, LHZ, LHD, and 2-basis
// material decomposition.  All algorithms operate on a pair of 16-bit
// grayscale projections (low-energy and high-energy) and produce an RGBA8
// pseudocolor output suitable for OpenGL texture upload.

#include <cstdint>

namespace DualEnergy {

// ── Enumerations ──────────────────────────────────────────────────────────────

// Which dual-energy algorithm to apply.
enum class Algorithm {
    LHR = 0,       // Low/High Ratio:  ratio = p_L / p_H (Z-sensitive)
    LHZ,           // Z-eff estimate:  PE/Compton decomposition → Z_eff
    LHD,           // Log Difference:  diff = p_L − k·p_H (basis suppression)
    MaterialDecomp // 2-basis decomp:  matrix inversion → thickness maps
};

// Pseudocolor mapping palette applied after the Z/ratio computation.
enum class Colorscheme {
    Security = 0,  // Orange (low-Z, organic) → Blue (high-Z, metal)
    Mining,        // Orange → Cyan-blue (industrial ore sorting)
    Grayscale      // Ratio/Z mapped to grey intensity only
};

// ── Parameter struct ──────────────────────────────────────────────────────────

struct Params {
    Algorithm   algo        = Algorithm::LHR;
    Colorscheme colorscheme = Colorscheme::Mining;

    // Air reference (flat-field) intensity for each channel.
    // 0 = auto: use the per-image maximum as a proxy.
    float ref_low  = 0.f;
    float ref_high = 0.f;

    // ── LHD-specific ──────────────────────────────────────────────────────
    // Suppression coefficient k: diff = p_L − k·p_H.
    // Set k = µ_L/µ_H of the material to suppress.
    //   Quartz / SiO₂  : k ≈ 1.15
    //   Aluminium       : k ≈ 1.28
    //   Organic / coal  : k ≈ 1.08
    float lhd_k = 1.15f;

    // ── LHZ-specific ──────────────────────────────────────────────────────
    // Compton coupling constant k_C (≈ f_C_L / f_C_H).
    //   60 kV / 140 kV pair: k_C ≈ 0.70
    //   80 kV / 160 kV pair: k_C ≈ 0.72
    float z_kC = 0.70f;
    // Z_eff calibration polynomial: Z_eff = a · R^b + c
    // R = PE-signal / (p_L + p_H), R ∈ [0, 1]
    float z_a = 6.0f;
    float z_b = 1.8f;
    float z_c = 2.0f;

    // ── MaterialDecomp-specific ───────────────────────────────────────────
    // Linear attenuation coefficients [cm⁻¹] of the two basis materials
    // at the effective low/high energies.
    // Default: basis pair (polyethylene ≈ low-Z, iron ≈ high-Z).
    float mu1L = 0.20f;  // material-1 at low  energy
    float mu1H = 0.10f;  // material-1 at high energy
    float mu2L = 1.20f;  // material-2 at low  energy
    float mu2H = 0.40f;  // material-2 at high energy

    // ── Display range ─────────────────────────────────────────────────────
    // The computed quantity (ratio / Z_eff / diff / fraction) is linearly
    // mapped from [disp_min, disp_max] onto the hue axis.
    float disp_min = 0.8f;
    float disp_max = 3.0f;

    // Brightness attenuation: V = exp(−att_scale · (p_L + p_H)).
    // Higher value → thicker regions appear darker.
    float att_scale = 0.5f;

    // Set to true to trigger re-render on the next RenderDualEnergy() call.
    bool dirty = true;
};

// ── Public API ────────────────────────────────────────────────────────────────

// Render a dual-energy pseudocolor image.
//
//   low, high : 16-bit projection buffers, each (width × height) pixels.
//   out       : pre-allocated RGBA8 output buffer (width × height × 4 bytes).
//
// The function is thread-safe and parallelised with OpenMP when available.
void Render(const uint16_t* low, const uint16_t* high,
            int width, int height,
            const Params& p,
            uint8_t* out);

} // namespace DualEnergy
