#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace ember
{
    enum class FilterType { off = 0, lowpass, highpass, bandpass, formant };

    // Zavalishin TPT state-variable filter (stereo) with tanh saturation on the input drive and in
    // the integrator state. Formant mode runs two normalised band-passes whose centres glide through
    // vowels (A-E-I-O-U) as the cutoff moves from low to high.
    class VoiceFilter
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            cutoffSm.reset (sr, 0.02);
            resSm.reset (sr, 0.02);
            driveSm.reset (sr, 0.02);
        }

        void reset() noexcept { for (auto& c : cores) c = {}; }

        bool isActive() const noexcept { return type != FilterType::off; }

        void setTargets (FilterType t, float cutoffHz, float resonance, float drive, bool snap)
        {
            type = t;
            if (snap)
            {
                cutoffSm.setCurrentAndTargetValue (cutoffHz);
                resSm.setCurrentAndTargetValue (resonance);
                driveSm.setCurrentAndTargetValue (drive);
                updateCoefficients (cutoffHz, resonance, drive);
            }
            else
            {
                cutoffSm.setTargetValue (cutoffHz);
                resSm.setTargetValue (resonance);
                driveSm.setTargetValue (drive);
            }
        }

        void process (float& l, float& r) noexcept
        {
            if (type == FilterType::off)
                return;

            if (--countdown <= 0)
            {
                countdown = updateInterval;
                updateCoefficients (cutoffSm.skip (updateInterval), resSm.skip (updateInterval), driveSm.skip (updateInterval));
            }

            const float inL = fastTanh (l * preGain);
            const float inR = fastTanh (r * preGain);

            if (type == FilterType::formant)
            {
                l = (tick (cores[0], 0, inL, 1) + tick (cores[1], 0, inL, 1)) * formantMakeup * postGain;
                r = (tick (cores[0], 1, inR, 1) + tick (cores[1], 1, inR, 1)) * formantMakeup * postGain;
                return;
            }

            const int mode = type == FilterType::lowpass ? 0 : type == FilterType::highpass ? 2 : 1;
            l = tick (cores[0], 0, inL, mode) * postGain;
            r = tick (cores[0], 1, inR, mode) * postGain;
        }

    private:
        struct Core
        {
            float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
            std::array<float, 2> ic1 {}, ic2 {};
        };

        static constexpr int updateInterval = 8;
        static constexpr float formantMakeup = 1.6f;

        static float fastTanh (float x) noexcept
        {
            x = juce::jlimit (-3.0f, 3.0f, x);
            return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
        }

        // mode: 0 = low, 1 = band (normalised, peak gain 1), 2 = high
        static float tick (Core& c, int ch, float v0, int mode) noexcept
        {
            const float v3 = v0 - c.ic2[(size_t) ch];
            const float v1 = c.a1 * c.ic1[(size_t) ch] + c.a2 * v3;
            const float v2 = c.ic2[(size_t) ch] + c.a2 * c.ic1[(size_t) ch] + c.a3 * v3;
            c.ic1[(size_t) ch] = fastTanh (2.0f * v1 - c.ic1[(size_t) ch]);
            c.ic2[(size_t) ch] = 2.0f * v2 - c.ic2[(size_t) ch];

            switch (mode)
            {
                case 0:  return v2;
                case 1:  return c.k * v1;
                default: return v0 - c.k * v1 - v2;
            }
        }

        static void setCore (Core& c, float hz, float k, double sr) noexcept
        {
            c.g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) (0.45 * sr), hz) / (float) sr);
            c.k = k;
            c.a1 = 1.0f / (1.0f + c.g * (c.g + k));
            c.a2 = c.g * c.a1;
            c.a3 = c.g * c.a2;
        }

        void updateCoefficients (float cutoffHz, float res, float drive) noexcept
        {
            preGain = 1.0f + drive * 9.0f;
            postGain = 1.0f / std::sqrt (preGain);

            if (type == FilterType::formant)
            {
                // F1/F2 for A E I O U
                static constexpr float f1[] { 800.0f, 400.0f, 270.0f, 450.0f, 325.0f };
                static constexpr float f2[] { 1150.0f, 2000.0f, 2140.0f, 800.0f, 700.0f };

                const float t = juce::jlimit (0.0f, 1.0f, std::log (cutoffHz / 20.0f) / std::log (1000.0f)) * 4.0f;
                const int i = juce::jmin ((int) t, 3);
                const float f = t - (float) i;
                const float k = 0.5f - 0.45f * res;
                setCore (cores[0], f1[i] + (f1[i + 1] - f1[i]) * f, k, sampleRate);
                setCore (cores[1], f2[i] + (f2[i + 1] - f2[i]) * f, k, sampleRate);
                return;
            }

            setCore (cores[0], cutoffHz, 2.0f - 1.96f * res, sampleRate);
        }

        double sampleRate = 44100.0;
        FilterType type = FilterType::off;
        std::array<Core, 2> cores;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSm { 1000.0f };
        juce::SmoothedValue<float> resSm, driveSm;
        float preGain = 1.0f, postGain = 1.0f;
        int countdown = 0;
    };
}
