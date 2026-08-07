#pragma once

#include "WavetableBank.h"
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace ember
{
    enum class WarpMode { off = 0, formant, sync, bend, phaseDist };

    // One wavetable oscillator with up to 16 stereo-spread unison voices.
    class WavetableOscillator
    {
    public:
        static constexpr int maxUnison = 16;

        explicit WavetableOscillator (const WavetableBank& b) : bank (b) {}

        void reset (bool randomisePhases)
        {
            juce::Random rng (0x5eed);
            for (int i = 0; i < maxUnison; ++i)
                phases[(size_t) i] = (randomisePhases && i > 0) ? rng.nextFloat() : 0.0f;
        }

        // Called once per block: recomputes pitch, mip level and stereo gains for each unison voice.
        void configure (double sampleRate, float baseHz, WarpMode warpMode, float warpAmountTarget,
                        int unison, float detuneCents, float spread)
        {
            mode = warpMode;
            count = juce::jlimit (1, maxUnison, unison);
            gainNorm = 1.0f / std::sqrt ((float) count);

            // Sync/formant multiply the effective playback rate; bend/PD add harmonics -> use a darker mip.
            const float ratio = (mode == WarpMode::sync || mode == WarpMode::formant) ? 1.0f + 7.0f * warpAmountTarget : 1.0f;
            const int mipBias = ((mode == WarpMode::bend || mode == WarpMode::phaseDist) && warpAmountTarget > 0.01f) ? 1 : 0;

            for (int u = 0; u < count; ++u)
            {
                const float pos = count > 1 ? 2.0f * (float) u / (float) (count - 1) - 1.0f : 0.0f;
                const double hz = (double) baseHz * std::pow (2.0, (double) (pos * detuneCents) / 1200.0);

                inc[(size_t) u] = (float) (hz / sampleRate);
                const float allowed = (float) (0.5 * sampleRate / (hz * ratio));
                mip[(size_t) u] = juce::jmin (WavetableBank::numMips - 1, WavetableBank::mipFor (allowed) + mipBias);

                const float pan = pos * spread;   // -1..1
                const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                gainL[(size_t) u] = std::cos (angle) * gainNorm * 1.41421356f;
                gainR[(size_t) u] = std::sin (angle) * gainNorm * 1.41421356f;
            }
        }

        // frame: 0..numFrames-1, warpAmount: 0..1. Accumulates into l/r.
        void process (float frame, float warpAmount, float& l, float& r) noexcept
        {
            const float ratio = 1.0f + 7.0f * warpAmount;

            for (int u = 0; u < count; ++u)
            {
                float ph = phases[(size_t) u];
                float q = ph, window = 1.0f;

                switch (mode)
                {
                    case WarpMode::off: break;

                    case WarpMode::formant:
                        q = ph * ratio;
                        q -= std::floor (q);
                        window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * ph);
                        break;

                    case WarpMode::sync:
                        q = ph * ratio;
                        q -= std::floor (q);
                        break;

                    case WarpMode::bend:
                    {
                        float bent;
                        if (ph < 0.5f) { const float x = 2.0f * ph;        bent = 0.5f * x * x * x; }
                        else           { const float y = 2.0f - 2.0f * ph; bent = 1.0f - 0.5f * y * y * y; }
                        q = ph + warpAmount * (bent - ph);
                        break;
                    }

                    case WarpMode::phaseDist:
                    {
                        const float b = 0.5f - 0.49f * warpAmount;   // break point moves toward 0
                        q = ph < b ? 0.5f * ph / b : 0.5f + 0.5f * (ph - b) / (1.0f - b);
                        break;
                    }
                }

                const float s = bank.sample (mip[(size_t) u], frame, q) * window;
                l += s * gainL[(size_t) u];
                r += s * gainR[(size_t) u];

                ph += inc[(size_t) u];
                phases[(size_t) u] = ph >= 1.0f ? ph - 1.0f : ph;
            }
        }

    private:
        const WavetableBank& bank;
        WarpMode mode = WarpMode::off;
        int count = 1;
        float gainNorm = 1.0f;
        std::array<float, maxUnison> phases {}, inc {}, gainL {}, gainR {};
        std::array<int, maxUnison> mip {};
    };
}
