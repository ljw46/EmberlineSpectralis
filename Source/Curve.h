#pragma once

#include <cmath>

namespace ember
{
    // Maps t in [0,1] to [0,1]. c > 0 moves fast first then eases (snappy), c < 0 eases in slowly, 0 is linear.
    inline float curveShape (float t, float c) noexcept
    {
        if (std::abs (c) < 1.0e-3f)
            return t;

        const float k = 5.0f * c;
        return (1.0f - std::exp (-k * t)) / (1.0f - std::exp (-k));
    }
}
