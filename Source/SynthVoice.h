#pragma once

#include "Parameters.h"
#include "SubNoise.h"
#include "VoiceFilter.h"
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

    // 3 wavetable oscillators + noise -> dual filters (serial/parallel) -> + sub (direct) -> amp ADSR.
    class SynthVoice final : public juce::SynthesiserVoice
    {
    public:
        SynthVoice (const WavetableBank& bank, const params::SynthParamPtrs& ptrs, uint32_t noiseSeed)
            : P (ptrs), oscParams (ptrs.osc), oscs { WavetableOscillator (bank), WavetableOscillator (bank), WavetableOscillator (bank) }
        {
            adsr.setParameters ({ 0.005f, 0.2f, 0.7f, 0.25f });
            noise[0].seed (noiseSeed * 2u + 1u);
            noise[1].seed (noiseSeed * 7919u + 13u);
        }

        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

        void setCurrentPlaybackSampleRate (double sr) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (sr);
            adsr.setSampleRate (sr);
            for (auto& s : smooth)
                for (auto* sm : { &s.level, &s.frame, &s.warp })
                    sm->reset (sr, 0.01);
            for (auto* sm : { &subLevelSm, &noiseLevelSm })
                sm->reset (sr, 0.01);
            for (auto& f : filters)
                f.prepare (sr);
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            baseHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
            velocityGain = velocity;

            for (int o = 0; o < params::numOscs; ++o)
            {
                oscs[(size_t) o].reset (oscParams[(size_t) o].unison->load() > 1.5f);
                auto& s = smooth[(size_t) o];
                s.level.setCurrentAndTargetValue (levelOf (oscParams[(size_t) o]));
                s.frame.setCurrentAndTargetValue (oscParams[(size_t) o].frame->load());
                s.warp.setCurrentAndTargetValue (oscParams[(size_t) o].warpAmt->load());
            }

            subLevelSm.setCurrentAndTargetValue (P.subLevel->load());
            noiseLevelSm.setCurrentAndTargetValue (P.noiseLevel->load());
            sub.reset();
            for (auto& n : noise) n.reset();
            for (size_t i = 0; i < filters.size(); ++i)
            {
                filters[i].reset();
                updateFilter (i, true);
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
                s.level.setTargetValue (levelOf (p));
                s.frame.setTargetValue (p.frame->load());
                s.warp.setTargetValue (p.warpAmt->load());

                const float semis = p.semi->load() + p.fine->load() * 0.01f;
                oscs[(size_t) o].configure (sr, baseHz * std::pow (2.0f, semis / 12.0f),
                                            (WarpMode) juce::roundToInt (p.warpMode->load()), s.warp.getTargetValue(),
                                            juce::roundToInt (p.unison->load()), p.detune->load(), p.spread->load());
            }

            subLevelSm.setTargetValue (P.subLevel->load());
            noiseLevelSm.setTargetValue (P.noiseLevel->load());
            sub.setFrequency (sr, baseHz, 1 + juce::roundToInt (P.subOct->load()));
            const bool subTri = juce::roundToInt (P.subWave->load()) == 1;
            const auto noiseType = (NoiseType) juce::roundToInt (P.noiseType->load());
            const bool parallel = juce::roundToInt (P.routing->load()) == 1;
            for (size_t i = 0; i < filters.size(); ++i)
                updateFilter (i, false);

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

                const auto nl = noiseLevelSm.getNextValue();
                if (nl > 0.0f)
                {
                    l += noise[0].next (noiseType, (float) sr) * nl;
                    r += noise[1].next (noiseType, (float) sr) * nl;
                }

                if (parallel)
                {
                    float al = l, ar = r, bl = l, br = r;
                    filters[0].process (al, ar);
                    filters[1].process (bl, br);
                    const bool on1 = filters[0].isActive(), on2 = filters[1].isActive();
                    if (on1 && on2)  { l = (al + bl) * 0.7071f; r = (ar + br) * 0.7071f; }
                    else if (on1)    { l = al; r = ar; }
                    else if (on2)    { l = bl; r = br; }
                }
                else
                {
                    filters[0].process (l, r);
                    filters[1].process (l, r);
                }

                const auto sl = subLevelSm.getNextValue();
                if (sl > 0.0f || subLevelSm.isSmoothing())
                {
                    const auto s = sub.next (subTri) * sl * subGain;
                    l += s;
                    r += s;
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

        static float levelOf (const params::OscParamPtrs& p) noexcept { return p.on->load() > 0.5f ? p.level->load() : 0.0f; }

        void updateFilter (size_t i, bool snap)
        {
            const auto& f = P.filter[i];
            filters[i].setTargets ((FilterType) juce::roundToInt (f.type->load()), f.cutoff->load(),
                                   f.res->load(), f.drive->load(), snap);
        }

        static constexpr float voiceGain = 0.3f;
        static constexpr float subGain = 1.0f;

        params::SynthParamPtrs P;
        SubOscillator sub;
        std::array<NoiseGenerator, 2> noise;
        std::array<VoiceFilter, 2> filters;
        juce::SmoothedValue<float> subLevelSm, noiseLevelSm;

        std::array<params::OscParamPtrs, params::numOscs> oscParams;
        std::array<WavetableOscillator, params::numOscs> oscs;
        std::array<OscSmoothing, params::numOscs> smooth;
        juce::ADSR adsr;
        float baseHz = 440.0f, velocityGain = 1.0f;
    };
}
