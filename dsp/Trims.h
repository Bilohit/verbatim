#pragma once
// Per-effect output trims in dB, applied to the wet path (docs/03-dsp-catalog.md section 3).
// Chosen at T20 to centre each effect at u = 32768 with -18 dBFS RMS pink noise on -18 dBFS (D12, +-3 allowed),
// capped at +6 dB and at 0 dB for PHASER/FLANGE so max-parameter peaks stay inside D4/F3 (ruling at T20).
#include "Catalog.h"

#include <array>

namespace vbdsp
{
inline constexpr std::array<float, vb::EFFECT_COUNT> trimDb { {
    -4.9f, // SAT
    0.9f, // CRUSH
    0.5f, // FILTER
    6.0f, // DELAY
    4.5f, // VERB (peak-limited, F3)
    5.1f, // CHORUS
    0.0f, // PHASER
    2.7f, // TREM
    1.4f, // WOW
    2.0f, // FREEZE
    0.2f, // COMB
    6.0f, // RING
    6.0f, // PITCH
    10.0f, // GRAIN
    -3.6f, // COMP
    0.0f, // FLANGE
} };
} // namespace vbdsp
