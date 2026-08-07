#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

namespace ember
{
    // Procedurally generated, mipmapped wavetable set. Every mip level is band-limited
    // (additive synthesis, harmonics truncated), so the oscillator picks the level whose
    // top harmonic stays below Nyquist for the note being played.
    class WavetableBank
    {
    public:
        static constexpr int tableSize = 2048;
        static constexpr int numFrames = 16;
        static constexpr int numMips   = 10;
        static constexpr int maxHarmonics = tableSize / 2 - 1;   // 1023 at mip 0, halved per level

        WavetableBank() { build(); }

        static constexpr int harmonicsForMip (int mip) { return maxHarmonics >> mip; }

        // Smallest (brightest) mip whose highest harmonic fits under `allowedHarmonics`.
        static int mipFor (float allowedHarmonics) noexcept
        {
            int m = 0;
            while (m < numMips - 1 && (float) harmonicsForMip (m) > allowedHarmonics)
                ++m;
            return m;
        }

        // frame in [0, numFrames-1], phase in [0, 1). Cubic Hermite along phase, linear across frames.
        float sample (int mip, float frame, float phase) const noexcept
        {
            int f0 = (int) frame;
            if (f0 > numFrames - 2) f0 = numFrames - 2;
            const float fr = frame - (float) f0;

            const float pos = phase * (float) tableSize;
            const int i = (int) pos;
            const float f = pos - (float) i;

            const auto a = hermite (table (mip, f0),     i, f);
            const auto b = hermite (table (mip, f0 + 1), i, f);
            return a + (b - a) * fr;
        }

    private:
        static constexpr int mask = tableSize - 1;

        const float* table (int mip, int frame) const noexcept
        {
            return data.data() + ((size_t) mip * numFrames + (size_t) frame) * tableSize;
        }

        static float hermite (const float* t, int i, float f) noexcept
        {
            const float y0 = t[(i - 1) & mask], y1 = t[i & mask],
                        y2 = t[(i + 1) & mask], y3 = t[(i + 2) & mask];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

        // Harmonic amplitudes (sine series) of the anchor shapes the frames morph between:
        // sine -> triangle -> saw -> square -> reedy/formant
        static float anchorAmp (int anchor, int h) noexcept
        {
            const float hf = (float) h;
            switch (anchor)
            {
                case 0:  return h == 1 ? 1.0f : 0.0f;
                case 1:  return (h & 1) ? (((h >> 1) & 1) ? -1.0f : 1.0f) / (hf * hf) : 0.0f;
                case 2:  return 1.0f / hf;
                case 3:  return (h & 1) ? 1.0f / hf : 0.0f;
                default: return (1.0f / hf) * (1.0f + 5.0f * std::exp (-((hf - 7.0f) * (hf - 7.0f)) / 12.0f));
            }
        }

        static float frameAmp (int frame, int h) noexcept
        {
            constexpr int numAnchors = 5;
            const float t = (float) frame / (float) (numFrames - 1) * (float) (numAnchors - 1);
            const int a = juce::jmin ((int) t, numAnchors - 2);
            const float fr = t - (float) a;
            return anchorAmp (a, h) * (1.0f - fr) + anchorAmp (a + 1, h) * fr;
        }

        void build()
        {
            data.assign ((size_t) numMips * numFrames * tableSize, 0.0f);

            std::array<float, tableSize> sine {};
            for (int n = 0; n < tableSize; ++n)
                sine[(size_t) n] = (float) std::sin (juce::MathConstants<double>::twoPi * n / tableSize);

            for (int frame = 0; frame < numFrames; ++frame)
            {
                std::array<float, maxHarmonics + 1> amps {};
                for (int h = 1; h <= maxHarmonics; ++h)
                    amps[(size_t) h] = frameAmp (frame, h);

                float norm = 1.0f;   // normalise to the full-bandwidth peak so all mips share one gain

                for (int mip = 0; mip < numMips; ++mip)
                {
                    auto* out = data.data() + ((size_t) mip * numFrames + (size_t) frame) * tableSize;
                    const int hMax = harmonicsForMip (mip);

                    for (int h = 1; h <= hMax; ++h)
                    {
                        const float a = amps[(size_t) h];
                        if (juce::exactlyEqual (a, 0.0f)) continue;
                        for (int n = 0; n < tableSize; ++n)
                            out[n] += a * sine[(size_t) ((h * n) & mask)];
                    }

                    if (mip == 0)
                    {
                        float peak = 1.0e-6f;
                        for (int n = 0; n < tableSize; ++n)
                            peak = juce::jmax (peak, std::abs (out[n]));
                        norm = 1.0f / peak;
                    }

                    for (int n = 0; n < tableSize; ++n)
                        out[n] *= norm;
                }
            }
        }

        std::vector<float> data;
    };
}
