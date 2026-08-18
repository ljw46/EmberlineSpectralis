#pragma once

#include "Envelope.h"
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

    // 3 wavetable oscillators + noise -> dual filters (serial/parallel) -> + sub (direct) -> amp (ENV 1).
    // Modulation is evaluated every `chunkSize` samples; the smoothers interpolate between chunks.
    class SynthVoice final : public juce::SynthesiserVoice
    {
    public:
        SynthVoice (const WavetableBank& bank, const params::SynthParamPtrs& ptrs, mod::ModContext& context, uint32_t noiseSeed)
            : P (ptrs), ctx (context), oscParams (ptrs.osc),
              oscs { WavetableOscillator (bank), WavetableOscillator (bank), WavetableOscillator (bank) }
        {
            noise[0].seed (noiseSeed * 2u + 1u);
            noise[1].seed (noiseSeed * 7919u + 13u);
        }

        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

        void setCurrentPlaybackSampleRate (double sr) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (sr);
            for (auto& e : envs)
                e.setSampleRate (sr);
            for (auto& s : smooth)
                for (auto* sm : { &s.level, &s.frame, &s.warp })
                    sm->reset (sr, 0.002);
            for (auto* sm : { &subLevelSm, &noiseLevelSm })
                sm->reset (sr, 0.002);
            for (auto& f : filters)
                f.prepare (sr);
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            baseHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
            velocityGain = velocity;
            snapNext = true;

            stamp = ++ctx.stampCounter;
            ctx.newestStamp.store (stamp);

            for (int o = 0; o < params::numOscs; ++o)
                oscs[(size_t) o].reset (oscParams[(size_t) o].unison->load() > 1.5f);

            sub.reset();
            for (auto& n : noise) n.reset();
            for (auto& f : filters) f.reset();
            lfoPhase.fill (0.0);

            loadEnvelopeParams();
            for (auto& e : envs)
                e.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                for (auto& e : envs)
                    e.noteOff();
            }
            else
            {
                for (auto& e : envs)
                    e.reset();
                finish();
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
            loadEnvelopeParams();
            loadSlots();

            const bool subTri = juce::roundToInt (P.subWave->load()) == 1;
            const auto noiseType = (NoiseType) juce::roundToInt (P.noiseType->load());
            const bool parallel = juce::roundToInt (P.routing->load()) == 1;
            sub.setFrequency (sr, baseHz, 1 + juce::roundToInt (P.subOct->load()));

            auto* left  = out.getWritePointer (0, start);
            auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

            for (int done = 0; done < num;)
            {
                const int n = juce::jmin (chunkSize, num - done);
                updateModulation (n, start + done, sr);

                for (int i = done; i < done + n; ++i)
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

                    const auto amp = envs[0].process (1) * velocityGain * voiceGain;
                    left[i] += l * amp;
                    if (right != nullptr)
                        right[i] += r * amp;
                }

                envs[1].process (n);
                envs[2].process (n);
                for (size_t k = 0; k < lfoPhase.size(); ++k)
                    lfoPhase[k] += n * lfoInc[k];

                done += n;

                if (! envs[0].isActive())
                {
                    finish();
                    return;
                }
            }
        }

    private:
        static constexpr int chunkSize = 32;
        static constexpr float voiceGain = 0.3f;
        static constexpr float subGain = 1.0f;

        struct OscSmoothing { juce::SmoothedValue<float> level, frame, warp; };

        void finish()
        {
            if (stamp == ctx.newestStamp.load())
                for (auto& v : ctx.liveOffset)
                    v.store (0.0f, std::memory_order_relaxed);
            clearCurrentNote();
        }

        void loadEnvelopeParams()
        {
            for (size_t i = 0; i < envs.size(); ++i)
            {
                const auto& e = P.mod.env[i];
                envs[i].setParams ({ e.attack->load(), e.decay->load(), e.sustain->load(), e.release->load(),
                                     e.aCurve->load(), e.dCurve->load() });
            }
        }

        void loadSlots()
        {
            numActiveSlots = 0;
            for (const auto& s : P.mod.slots)
            {
                const int src = juce::roundToInt (s.src->load());
                const int dst = juce::roundToInt (s.dst->load()) - 1;
                const float depth = s.depth->load();
                if (src > 0 && dst >= 0 && std::abs (depth) > 1.0e-4f)
                {
                    slotSrc[(size_t) numActiveSlots] = src;
                    slotDst[(size_t) numActiveSlots] = dst;
                    slotDepth[(size_t) numActiveSlots] = depth;
                    ++numActiveSlots;
                }
            }

            const double sr = getSampleRate();
            for (size_t i = 0; i < lfoInc.size(); ++i)
            {
                lfoInc[i] = (double) P.mod.lfo[i].rate->load() / sr;
                lfoFree[i] = P.mod.lfo[i].mode->load() > 0.5f;
            }
        }

        // Applies the summed modulation for destination `d` to its base value, in the parameter's
        // normalised (skewed) domain so a cutoff sweep sounds even across the range.
        float eff (int d, float base) const noexcept
        {
            const float o = offsets[(size_t) d];
            if (juce::exactlyEqual (o, 0.0f))
                return base;

            const auto& r = P.mod.destRange[(size_t) d];
            return r.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, r.convertTo0to1 (base) + o));
        }

        template <typename Smoother>
        void aim (Smoother& s, float v, bool snap) { if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v); }

        void updateModulation (int n, int absoluteStart, double sr)
        {
            const bool snap = snapNext;
            snapNext = false;

            // sources
            std::array<float, mod::numSources> src {};
            for (int i = 0; i < mod::numLfos; ++i)
            {
                const double ph = lfoFree[(size_t) i] ? ctx.blockPhase[(size_t) i] + (double) absoluteStart * lfoInc[(size_t) i]
                                                      : lfoPhase[(size_t) i];
                src[(size_t) (mod::srcLfo + i)] = ctx.shapes.read (i, ph);
            }
            for (int i = 0; i < mod::numEnvs; ++i)
                src[(size_t) (mod::srcEnv + i)] = envs[(size_t) i].value();
            for (int i = 0; i < mod::numMacros; ++i)
                src[(size_t) (mod::srcMacro + i)] = P.mod.macro[(size_t) i]->load();

            offsets.fill (0.0f);
            for (int s = 0; s < numActiveSlots; ++s)
                offsets[(size_t) slotDst[(size_t) s]] += slotDepth[(size_t) s] * src[(size_t) slotSrc[(size_t) s]];

            if (stamp == ctx.newestStamp.load())
            {
                for (size_t d = 0; d < offsets.size(); ++d)
                    ctx.liveOffset[d].store (offsets[d], std::memory_order_relaxed);
                for (size_t i = 0; i < lfoFree.size(); ++i)
                    if (! lfoFree[i])
                        ctx.displayPhase[i].store ((float) (lfoPhase[i] - std::floor (lfoPhase[i])), std::memory_order_relaxed);
            }

            // oscillators
            for (int o = 0; o < params::numOscs; ++o)
            {
                const auto& p = oscParams[(size_t) o];
                auto& s = smooth[(size_t) o];

                const float level = p.on->load() > 0.5f ? eff (mod::oscDest (o, 2), p.level->load()) : 0.0f;
                const float warp = eff (mod::oscDest (o, 1), p.warpAmt->load());
                aim (s.level, level, snap);
                aim (s.frame, eff (mod::oscDest (o, 0), p.frame->load()), snap);
                aim (s.warp, warp, snap);

                if (level > 0.0f || s.level.isSmoothing())
                {
                    const float semis = p.semi->load() + eff (mod::oscDest (o, 5), p.fine->load()) * 0.01f;
                    oscs[(size_t) o].configure (sr, baseHz * std::pow (2.0f, semis / 12.0f),
                                                (WarpMode) juce::roundToInt (p.warpMode->load()), warp,
                                                juce::roundToInt (p.unison->load()),
                                                eff (mod::oscDest (o, 3), p.detune->load()),
                                                eff (mod::oscDest (o, 4), p.spread->load()));
                }
            }

            // filters, sub, noise
            for (int f = 0; f < 2; ++f)
            {
                const auto& fp = P.filter[(size_t) f];
                filters[(size_t) f].setTargets ((FilterType) juce::roundToInt (fp.type->load()),
                                                eff (mod::filterDest (f, 0), fp.cutoff->load()),
                                                eff (mod::filterDest (f, 1), fp.res->load()),
                                                eff (mod::filterDest (f, 2), fp.drive->load()), snap);
            }

            aim (subLevelSm, eff (mod::subDest, P.subLevel->load()), snap);
            aim (noiseLevelSm, eff (mod::noiseDest, P.noiseLevel->load()), snap);
            juce::ignoreUnused (n);
        }

        params::SynthParamPtrs P;
        mod::ModContext& ctx;
        std::array<params::OscParamPtrs, params::numOscs> oscParams;
        std::array<WavetableOscillator, params::numOscs> oscs;
        std::array<OscSmoothing, params::numOscs> smooth;
        SubOscillator sub;
        std::array<NoiseGenerator, 2> noise;
        std::array<VoiceFilter, 2> filters;
        std::array<Envelope, mod::numEnvs> envs;
        juce::SmoothedValue<float> subLevelSm, noiseLevelSm;

        std::array<float, mod::numDests> offsets {};
        std::array<int, mod::numSlots> slotSrc {}, slotDst {};
        std::array<float, mod::numSlots> slotDepth {};
        int numActiveSlots = 0;

        std::array<double, mod::numLfos> lfoPhase {}, lfoInc {};
        std::array<bool, mod::numLfos> lfoFree {};

        std::uint64_t stamp = 0;
        bool snapNext = false;
        float baseHz = 440.0f, velocityGain = 1.0f;
    };
}
