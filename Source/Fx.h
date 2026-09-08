#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// Serial FX chain: distortion -> 4-stage chorus/flanger -> ping-pong delay -> FDN reverb.
namespace ember::fx
{
    inline const juce::StringArray distTypeNames   { "Soft", "Hard", "Fold", "Tube" };
    inline const juce::StringArray chorusModeNames { "Chorus", "Flanger" };
    inline const juce::StringArray effectNames     { "Distortion", "Chorus/Flanger", "Delay", "Reverb" };
    inline constexpr int numEffects = 4;

    struct Ptrs
    {
        std::atomic<float> *distOn, *distType, *distDrive, *distTone, *distMix;
        std::atomic<float> *chOn, *chMode, *chRate, *chDepth, *chFeedback, *chMix;
        std::atomic<float> *dlOn, *dlTime, *dlFeedback, *dlTone, *dlMix;
        std::atomic<float> *rvOn, *rvSize, *rvDecay, *rvDamp, *rvMix;
        std::array<std::atomic<float>*, numEffects> slot;   // which effect sits in each chain position
    };

    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p)
    {
        using namespace juce;
        using Range = NormalisableRange<float>;

        auto toggle = [&] (const char* id, const char* name) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, false)); };
        auto choice = [&] (const char* id, const char* name, const StringArray& items) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, 0)); };
        auto knob = [&] (const char* id, const char* name, Range r, float def, const char* unit = "")
        {
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit)));
        };

        toggle ("fx_dist_on", "Distortion On");
        choice ("fx_dist_type", "Distortion Type", distTypeNames);
        knob ("fx_dist_drive", "Distortion Drive", { 0.0f, 1.0f, 0.001f }, 0.3f);
        knob ("fx_dist_tone", "Distortion Tone", { 0.0f, 1.0f, 0.001f }, 0.7f);
        knob ("fx_dist_mix", "Distortion Mix", { 0.0f, 1.0f, 0.001f }, 1.0f);

        toggle ("fx_chorus_on", "Chorus On");
        choice ("fx_chorus_mode", "Chorus Mode", chorusModeNames);
        knob ("fx_chorus_rate", "Chorus Rate", { 0.05f, 8.0f, 0.001f, 0.4f }, 0.6f, "Hz");
        knob ("fx_chorus_depth", "Chorus Depth", { 0.0f, 1.0f, 0.001f }, 0.5f);
        knob ("fx_chorus_feedback", "Chorus Feedback", { 0.0f, 0.85f, 0.001f }, 0.0f);
        knob ("fx_chorus_mix", "Chorus Mix", { 0.0f, 1.0f, 0.001f }, 0.5f);

        toggle ("fx_delay_on", "Delay On");
        knob ("fx_delay_time", "Delay Time", { 10.0f, 2000.0f, 0.1f, 0.4f }, 375.0f, "ms");
        knob ("fx_delay_feedback", "Delay Feedback", { 0.0f, 0.95f, 0.001f }, 0.4f);
        knob ("fx_delay_tone", "Delay Tone", { 300.0f, 16000.0f, 1.0f, 0.4f }, 5000.0f, "Hz");
        knob ("fx_delay_mix", "Delay Mix", { 0.0f, 1.0f, 0.001f }, 0.3f);

        toggle ("fx_reverb_on", "Reverb On");
        knob ("fx_reverb_size", "Reverb Size", { 0.0f, 1.0f, 0.001f }, 0.5f);
        knob ("fx_reverb_decay", "Reverb Decay", { 0.2f, 20.0f, 0.01f, 0.4f }, 3.0f, "s");
        knob ("fx_reverb_damp", "Reverb Damping", { 0.0f, 1.0f, 0.001f }, 0.4f);
        knob ("fx_reverb_mix", "Reverb Mix", { 0.0f, 1.0f, 0.001f }, 0.3f);

        for (int i = 0; i < numEffects; ++i)
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "fx_slot" + String (i + 1), 1 },
                                                                  "FX Slot " + String (i + 1), effectNames, i));
    }

    inline Ptrs lookup (juce::AudioProcessorValueTreeState& a)
    {
        auto g = [&] (const char* id) { return a.getRawParameterValue (id); };
        return { g ("fx_dist_on"), g ("fx_dist_type"), g ("fx_dist_drive"), g ("fx_dist_tone"), g ("fx_dist_mix"),
                 g ("fx_chorus_on"), g ("fx_chorus_mode"), g ("fx_chorus_rate"), g ("fx_chorus_depth"), g ("fx_chorus_feedback"), g ("fx_chorus_mix"),
                 g ("fx_delay_on"), g ("fx_delay_time"), g ("fx_delay_feedback"), g ("fx_delay_tone"), g ("fx_delay_mix"),
                 g ("fx_reverb_on"), g ("fx_reverb_size"), g ("fx_reverb_decay"), g ("fx_reverb_damp"), g ("fx_reverb_mix"),
                 { g ("fx_slot1"), g ("fx_slot2"), g ("fx_slot3"), g ("fx_slot4") } };
    }

    namespace detail
    {
        inline int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

        // Circular buffer with linearly interpolated fractional-delay reads.
        struct DelayLine
        {
            void init (int minSamples) { buf.assign ((size_t) nextPow2 (minSamples + 4), 0.0f); mask = (int) buf.size() - 1; w = 0; }
            void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
            void write (float x) noexcept { buf[(size_t) (w & mask)] = x; ++w; }

            // delay in samples (>= 0): 0 returns the sample just written
            float read (float delay) const noexcept
            {
                const float p = (float) (w - 1) - delay;
                const float fl = std::floor (p);
                const int i = (int) fl;
                const float f = p - fl;
                return buf[(size_t) (i & mask)] * (1.0f - f) + buf[(size_t) ((i + 1) & mask)] * f;
            }

            std::vector<float> buf;
            int mask = 0, w = 0;
        };

        struct Allpass
        {
            void init (int len) { line.init (len + 2); delay = len; line.clear(); }
            float process (float x, float g) noexcept
            {
                const float d = line.read ((float) delay - 1.0f);
                const float v = x + g * d;
                line.write (v);
                return d - g * v;
            }
            DelayLine line;
            int delay = 1;
        };

        inline float shape (int type, float x) noexcept
        {
            switch (type)
            {
                case 0:  { x = juce::jlimit (-3.0f, 3.0f, x); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }   // soft (tanh-like)
                case 1:  return juce::jlimit (-1.0f, 1.0f, x);                                                           // hard clip
                case 2:  { float p = x * 0.25f + 0.25f; p -= std::floor (p); return 1.0f - 4.0f * std::abs (p - 0.5f); } // triangle wavefold
                default: { auto t = [] (float v) { v = juce::jlimit (-3.0f, 3.0f, v); return v * (27.0f + v * v) / (27.0f + 9.0f * v * v); };
                           return t (x + 0.3f) - t (0.3f); }                                                              // asymmetric "tube"
            }
        }

        // Smooths the wet mix and says when an effect can be skipped entirely.
        struct Gate
        {
            void prepare (double sr) { mix.reset (sr, 0.02); mix.setCurrentAndTargetValue (0.0f); active = false; }
            // returns true if the effect should run this block
            bool update (bool on, float mixAmount)
            {
                mix.setTargetValue (on ? mixAmount : 0.0f);
                const bool run = on || mix.isSmoothing() || mix.getCurrentValue() > 1.0e-4f;
                const bool woke = run && ! active;
                active = run;
                needsReset = ! run && wasRunning;
                wasRunning = run;
                justWoke = woke;
                return run;
            }
            juce::SmoothedValue<float> mix;
            bool active = false, wasRunning = false, needsReset = false, justWoke = false;
        };
    }

    class Distortion
    {
    public:
        void prepare (double sr, int maxBlock)
        {
            sampleRate = sr;
            os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            os->initProcessing ((size_t) maxBlock);
            latency = juce::roundToInt (os->getLatencyInSamples());
            dryBuf.setSize (2, maxBlock);
            for (auto& d : dry) d.init (latency + 8);
            gate.prepare (sr);
            gain.reset (sr * 2.0, 0.02);
            tone.reset (sr, 0.02);
            reset();
        }

        int getLatency() const noexcept { return latency; }

        void reset()
        {
            os->reset();
            toneState.fill (0.0f);
            dcX.fill (0.0f);
            dcY.fill (0.0f);
            for (auto& d : dry) d.clear();
            gain.setCurrentAndTargetValue (1.0f);
        }

        void process (juce::AudioBuffer<float>& b, const Ptrs& p)
        {
            const int n = b.getNumSamples();
            const bool on = p.distOn->load() > 0.5f;
            const bool run = gate.update (on, p.distMix->load());
            const int type = juce::roundToInt (p.distType->load());
            const float drive = p.distDrive->load();

            // the dry path is always delayed by the oversampler latency so the chain stays time-aligned
            if (! run)
            {
                for (int c = 0; c < 2; ++c)
                {
                    auto* x = b.getWritePointer (c);
                    for (int i = 0; i < n; ++i)
                    {
                        dry[(size_t) c].write (x[i]);
                        x[i] = dry[(size_t) c].read ((float) latency);
                    }
                }
                if (gate.needsReset) reset();
                return;
            }

            dryBuf.setSize (2, n, false, false, true);
            dryBuf.copyFrom (0, 0, b, 0, 0, n);
            dryBuf.copyFrom (1, 0, b, 1, 0, n);

            const float g = 1.0f + drive * 29.0f;
            gain.setTargetValue (g);
            tone.setTargetValue (p.distTone->load());

            juce::dsp::AudioBlock<float> block (b);
            auto up = os->processSamplesUp (block);
            const auto upN = up.getNumSamples();
            for (size_t i = 0; i < upN; ++i)
            {
                const float gi = gain.getNextValue();
                const float comp = 1.0f / std::sqrt (gi);
                for (size_t c = 0; c < 2; ++c)
                {
                    auto* x = up.getChannelPointer (c);
                    x[i] = detail::shape (type, x[i] * gi) * comp;
                }
            }
            os->processSamplesDown (block);

            for (int i = 0; i < n; ++i)
            {
                const float t = tone.getNextValue();
                const float cutoff = 700.0f * std::pow (26.0f, t);   // 700 Hz .. 18 kHz
                const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sampleRate);
                const float m = gate.mix.getNextValue();

                for (int c = 0; c < 2; ++c)
                {
                    auto* x = b.getWritePointer (c);
                    const auto cs = (size_t) c;

                    // DC blocker, then tone low-pass
                    const float dc = x[i] - dcX[cs] + 0.995f * dcY[cs];
                    dcX[cs] = x[i];
                    dcY[cs] = dc;
                    toneState[cs] += a * (dc - toneState[cs]);

                    dry[cs].write (dryBuf.getSample (c, i));
                    x[i] = dry[cs].read ((float) latency) * (1.0f - m) + toneState[cs] * m;
                }
            }
        }

    private:
        double sampleRate = 44100.0;
        int latency = 0;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        std::array<detail::DelayLine, 2> dry;
        juce::AudioBuffer<float> dryBuf;
        detail::Gate gate;
        juce::SmoothedValue<float> gain, tone;
        std::array<float, 2> toneState {}, dcX {}, dcY {};
    };

    class ChorusFlanger
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            for (auto& d : line) d.init ((int) (0.06 * sr));
            gate.prepare (sr);
            for (auto* s : { &rate, &depth, &feedback })
                s->reset (sr, 0.03);
            reset();
        }

        void reset() { for (auto& d : line) d.clear(); phase = 0.0f; fbState = { 0.0f, 0.0f }; }

        void process (juce::AudioBuffer<float>& b, const Ptrs& p)
        {
            const bool run = gate.update (p.chOn->load() > 0.5f, p.chMix->load());
            if (! run) { if (gate.needsReset) reset(); return; }

            const bool flanger = p.chMode->load() > 0.5f;
            rate.setTargetValue (p.chRate->load());
            depth.setTargetValue (p.chDepth->load());
            feedback.setTargetValue (p.chFeedback->load());

            const float baseMs = flanger ? 3.0f : 14.0f, rangeMs = flanger ? 2.5f : 8.0f;
            const float msToSamples = (float) sampleRate * 0.001f;
            const int n = b.getNumSamples();

            for (int i = 0; i < n; ++i)
            {
                phase += rate.getNextValue() / (float) sampleRate;
                phase -= std::floor (phase);
                const float d = depth.getNextValue(), fb = feedback.getNextValue(), m = gate.mix.getNextValue();

                for (int c = 0; c < 2; ++c)
                {
                    auto* x = b.getWritePointer (c);
                    const auto cs = (size_t) c;
                    const float in = x[i];

                    line[cs].write (in + fb * fbState[cs]);

                    float wet = 0.0f;
                    for (int k = 0; k < 4; ++k)   // four taps, 90 degrees apart; right channel offset by 45
                    {
                        const float ph = phase + 0.25f * (float) k + (c == 1 ? 0.125f : 0.0f);
                        const float ms = baseMs + rangeMs * d * std::sin (juce::MathConstants<float>::twoPi * ph);
                        wet += line[cs].read (ms * msToSamples);
                    }
                    wet *= 0.25f;
                    fbState[cs] = wet;
                    x[i] = in * (1.0f - m) + wet * m;
                }
            }
        }

    private:
        double sampleRate = 44100.0;
        std::array<detail::DelayLine, 2> line;
        detail::Gate gate;
        juce::SmoothedValue<float> rate, depth, feedback;
        float phase = 0.0f;
        std::array<float, 2> fbState {};
    };

    // Ping-pong: left tap at t, right at 2t, left at 3t ... the right line is fed by the left line's output.
    class PingPongDelay
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            for (auto& d : line) d.init ((int) (2.1 * sr));
            gate.prepare (sr);
            time.reset (sr, 0.15);
            feedback.reset (sr, 0.03);
            reset();
        }

        void reset() { for (auto& d : line) d.clear(); lp = 0.0f; time.setCurrentAndTargetValue (time.getTargetValue()); }

        void process (juce::AudioBuffer<float>& b, const Ptrs& p)
        {
            const bool run = gate.update (p.dlOn->load() > 0.5f, p.dlMix->load());
            if (! run) { if (gate.needsReset) reset(); return; }

            if (gate.justWoke)
                time.setCurrentAndTargetValue (p.dlTime->load());
            time.setTargetValue (p.dlTime->load());
            feedback.setTargetValue (p.dlFeedback->load());
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.dlTone->load() / (float) sampleRate);

            auto* l = b.getWritePointer (0);
            auto* r = b.getWritePointer (1);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float t = juce::jmax (1.0f, time.getNextValue() * 0.001f * (float) sampleRate);
                const float fb = feedback.getNextValue(), m = gate.mix.getNextValue();

                const float dl = line[0].read (t - 1.0f), dr = line[1].read (t - 1.0f);
                lp += a * (dr - lp);

                const float in = 0.5f * (l[i] + r[i]);
                line[0].write (in + fb * lp);
                line[1].write (dl);

                l[i] += dl * m;
                r[i] += dr * m;
            }
        }

    private:
        double sampleRate = 44100.0;
        std::array<detail::DelayLine, 2> line;
        detail::Gate gate;
        juce::SmoothedValue<float> time, feedback;
        float lp = 0.0f;
    };

    // 8-line feedback delay network (Householder mixing), modulated line lengths, damped feedback,
    // and two allpass diffusers per channel on the input.
    class Reverb
    {
    public:
        static constexpr int N = 8;

        void prepare (double sr)
        {
            sampleRate = sr;
            const float k = (float) (sr / 44100.0);
            static constexpr int base[N] { 1433, 1601, 1867, 2053, 2251, 2399, 2579, 2711 };
            for (int i = 0; i < N; ++i)
            {
                baseLen[(size_t) i] = (float) base[i] * k;
                lines[(size_t) i].init ((int) (baseLen[(size_t) i] * 1.7f * 1.2f) + 64);
            }
            static constexpr int apL[2] { 142, 379 }, apR[2] { 107, 277 };
            for (int i = 0; i < 2; ++i)
            {
                diffL[(size_t) i].init ((int) ((float) apL[i] * k));
                diffR[(size_t) i].init ((int) ((float) apR[i] * k));
            }
            gate.prepare (sr);
            size.reset (sr, 0.2);
            reset();
        }

        void reset()
        {
            for (auto& l : lines) l.clear();
            for (auto& d : diffL) d.line.clear();
            for (auto& d : diffR) d.line.clear();
            damp.fill (0.0f);
            for (int i = 0; i < N; ++i) lfoPhase[(size_t) i] = 0.125f * (float) i;
        }

        void process (juce::AudioBuffer<float>& b, const Ptrs& p)
        {
            const bool run = gate.update (p.rvOn->load() > 0.5f, p.rvMix->load());
            if (! run) { if (gate.needsReset) reset(); return; }

            if (gate.justWoke)
                size.setCurrentAndTargetValue (p.rvSize->load());
            size.setTargetValue (p.rvSize->load());

            const float decay = p.rvDecay->load();
            const float dampA = 1.0f - p.rvDamp->load() * 0.85f;
            const float modDepth = 6.0f * (float) (sampleRate / 44100.0);
            const float sr = (float) sampleRate;

            auto* l = b.getWritePointer (0);
            auto* r = b.getWritePointer (1);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float scale = 0.6f + size.getNextValue();   // 0.6 .. 1.6
                const float m = gate.mix.getNextValue();

                const float inL = diffL[1].process (diffL[0].process (l[i], 0.6f), 0.6f);
                const float inR = diffR[1].process (diffR[0].process (r[i], 0.6f), 0.6f);

                std::array<float, N> v {}, f {};
                float sum = 0.0f, outL = 0.0f, outR = 0.0f;
                for (int k = 0; k < N; ++k)
                {
                    const auto ks = (size_t) k;
                    lfoPhase[ks] += (0.11f + 0.043f * (float) k) / sr;
                    lfoPhase[ks] -= std::floor (lfoPhase[ks]);
                    const float len = baseLen[ks] * scale;
                    v[ks] = lines[ks].read (len + modDepth * std::sin (juce::MathConstants<float>::twoPi * lfoPhase[ks]));

                    damp[ks] += dampA * (v[ks] - damp[ks]);
                    const float g = std::pow (10.0f, -3.0f * len / (decay * sr));   // -60 dB after `decay` seconds
                    f[ks] = damp[ks] * g;
                    sum += f[ks];
                    (k & 1 ? outR : outL) += v[ks];
                }

                sum *= 2.0f / (float) N;
                for (int k = 0; k < N; ++k)
                    lines[(size_t) k].write (f[(size_t) k] - sum + 0.3f * (k & 1 ? inR : inL));

                const float wl = outL * 0.35f, wr = outR * 0.35f;
                l[i] = l[i] * (1.0f - 0.3f * m) + wl * m;
                r[i] = r[i] * (1.0f - 0.3f * m) + wr * m;
            }
        }

    private:
        double sampleRate = 44100.0;
        std::array<detail::DelayLine, N> lines;
        std::array<detail::Allpass, 2> diffL, diffR;
        std::array<float, N> baseLen {}, damp {}, lfoPhase {};
        detail::Gate gate;
        juce::SmoothedValue<float> size;
    };

    class Chain
    {
    public:
        void prepare (double sr, int maxBlock)
        {
            dist.prepare (sr, maxBlock);
            chorus.prepare (sr);
            delay.prepare (sr);
            reverb.prepare (sr);
        }

        int getLatency() const noexcept { return dist.getLatency(); }

        void process (juce::AudioBuffer<float>& b, const Ptrs& p)
        {
            // chain order comes from the slot params; a duplicate (mid-swap) just runs once and any
            // missing effect is appended, so the order is always a valid permutation
            std::array<bool, numEffects> used {};
            std::array<int, numEffects> order {};
            int count = 0;
            for (auto* slot : p.slot)
            {
                const int e = juce::jlimit (0, numEffects - 1, juce::roundToInt (slot->load()));
                if (! used[(size_t) e]) { used[(size_t) e] = true; order[(size_t) count++] = e; }
            }
            for (int e = 0; e < numEffects; ++e)
                if (! used[(size_t) e]) order[(size_t) count++] = e;

            for (int e : order)
            {
                switch (e)
                {
                    case 0:  dist.process (b, p); break;
                    case 1:  chorus.process (b, p); break;
                    case 2:  delay.process (b, p); break;
                    default: reverb.process (b, p); break;
                }
            }
        }

    private:
        Distortion dist;
        ChorusFlanger chorus;
        PingPongDelay delay;
        Reverb reverb;
    };
}
