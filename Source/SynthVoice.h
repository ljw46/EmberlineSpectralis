#pragma once

#include "Parameters.h"
#include "WavetableOscillator.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace ember
{
    struct SynthSound final : juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    // Three wavetable oscillators -> amp ADSR. Sub/noise, filters and modulation join later.
    class SynthVoice final : public juce::SynthesiserVoice
    {
    public:
        SynthVoice (const WavetableBank& bank, const std::array<params::OscParamPtrs, params::numOscs>& ptrs)
            : oscParams (ptrs), oscs { WavetableOscillator (bank), WavetableOscillator (bank), WavetableOscillator (bank) }
        {
            adsr.setParameters ({ 0.005f, 0.2f, 0.7f, 0.25f });
        }

        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

        void setCurrentPlaybackSampleRate (double sr) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (sr);
            adsr.setSampleRate (sr);
            for (auto& s : smooth)
                for (auto* sm : { &s.level, &s.frame, &s.warp })
                    sm->reset (sr, 0.01);
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            baseHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
            velocityGain = velocity;

            for (int o = 0; o < params::numOscs; ++o)
            {
                oscs[(size_t) o].reset (oscParams[(size_t) o].unison->load() > 1.5f);
                auto& s = smooth[(size_t) o];
                s.level.setCurrentAndTargetValue (oscParams[(size_t) o].level->load());
                s.frame.setCurrentAndTargetValue (oscParams[(size_t) o].frame->load());
                s.warp.setCurrentAndTargetValue (oscParams[(size_t) o].warpAmt->load());
            }
            adsr.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
                adsr.noteOff();
            else
            {
                adsr.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        using juce::SynthesiserVoice::renderNextBlock;

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive())
                return;

            const auto sr = getSampleRate();

            for (int o = 0; o < params::numOscs; ++o)
            {
                const auto& p = oscParams[(size_t) o];
                auto& s = smooth[(size_t) o];
                s.level.setTargetValue (p.level->load());
                s.frame.setTargetValue (p.frame->load());
                s.warp.setTargetValue (p.warpAmt->load());

                const float semis = p.semi->load() + p.fine->load() * 0.01f;
                oscs[(size_t) o].configure (sr, baseHz * std::pow (2.0f, semis / 12.0f),
                                            (WarpMode) juce::roundToInt (p.warpMode->load()), s.warp.getTargetValue(),
                                            juce::roundToInt (p.unison->load()), p.detune->load(), p.spread->load());
            }

            auto* left  = out.getWritePointer (0, start);
            auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

            for (int i = 0; i < num; ++i)
            {
                float l = 0.0f, r = 0.0f;

                for (int o = 0; o < params::numOscs; ++o)
                {
                    auto& s = smooth[(size_t) o];
                    if (! (s.level.isSmoothing() || s.level.getTargetValue() > 0.0f))
                        continue;

                    float ol = 0.0f, orr = 0.0f;
                    const auto frame = s.frame.getNextValue() * (float) (WavetableBank::numFrames - 1);
                    oscs[(size_t) o].process (frame, s.warp.getNextValue(), ol, orr);

                    const auto g = s.level.getNextValue();
                    l += ol * g;
                    r += orr * g;
                }

                const auto amp = adsr.getNextSample() * velocityGain * voiceGain;
                left[i] += l * amp;
                if (right != nullptr)
                    right[i] += r * amp;
            }

            if (! adsr.isActive())
                clearCurrentNote();
        }

    private:
        struct OscSmoothing { juce::SmoothedValue<float> level, frame, warp; };

        static constexpr float voiceGain = 0.3f;

        std::array<params::OscParamPtrs, params::numOscs> oscParams;
        std::array<WavetableOscillator, params::numOscs> oscs;
        std::array<OscSmoothing, params::numOscs> smooth;
        juce::ADSR adsr;
        float baseHz = 440.0f, velocityGain = 1.0f;
    };
}
