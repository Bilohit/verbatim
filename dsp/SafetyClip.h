#pragma once
#include <cmath>

namespace vbdsp
{
// Final output stage (docs/03-dsp-catalog.md section 1.4): transparent up to 0.7079, tanh knee, clamp.
inline float safetyClip (float x)
{
    const float a = std::abs (x);
    if (a <= 0.7079f)
        return x;
    float y = 0.7079f + 0.2921f * std::tanh ((a - 0.7079f) / 0.2921f);
    y = std::fmin (y, 1.0f);
    return x < 0.0f ? -y : y;
}
} // namespace vbdsp
