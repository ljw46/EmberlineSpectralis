#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>

namespace ember
{
    enum class NoiseType { white = 0, pink, crackle };

    // Cheap xorshift noise source; one per channel so the stereo image stays decorrelated.
    class NoiseGenerator
    {
    public:
        void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9e3779b9u; reset(); }

        void reset() noexcept
        {
            b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
            click = 0.0f;
        }

        float next (NoiseType type, float sampleRate) noexcept
        {
            switch (type)
            {
                case NoiseType::white:
                    return white() * 0.5f;

                case NoiseType::pink:   // Paul Kellet's economy filter
                {
                    const float w = white();
                    b0 = 0.99886f * b0 + w * 0.0555179f;
                    b1 = 0.99332f * b1 + w * 0.0750759f;
                    b2 = 0.96900f * b2 + w * 0.1538520f;
                    b3 = 0.86650f * b3 + w * 0.3104856f;
                    b4 = 0.55000f * b4 + w * 0.5329522f;
                    b5 = -0.7616f * b5 - w * 0.0168980f;
                    const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
                    b6 = w * 0.115926f;
                    return out * 0.11f;
                }

                case NoiseType::crackle:   // sparse, randomly sized clicks with a fast decay (vinyl-ish)
                {
                    if (uniform() < 250.0f / sampleRate)
                        click = white();
                    const float out = click;
                    click *= 0.82f;
                    return out * 1.2f;
                }
            }
            return 0.0f;
        }

    private:
        uint32_t nextBits() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        float uniform() noexcept { return (float) (nextBits() >> 8) * (1.0f / 16777216.0f); }   // [0, 1)
        float white() noexcept   { return uniform() * 2.0f - 1.0f; }

        uint32_t state = 0x12345678u;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, click = 0;
    };

    class SubOscillator
    {
    public:
        void reset() noexcept { phase = 0.0f; }

        // octaveDown: 1 or 2
        void setFrequency (double sampleRate, float noteHz, int octaveDown) noexcept
        {
            inc = (float) ((double) noteHz / sampleRate) / (float) (1 << octaveDown);
        }

        float next (bool triangle) noexcept
        {
            const float out = triangle ? 4.0f * std::abs (phase - 0.5f) - 1.0f
                                       : std::sin (juce::MathConstants<float>::twoPi * phase);
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            return out;
        }

    private:
        float phase = 0.0f, inc = 0.0f;
    };
}
