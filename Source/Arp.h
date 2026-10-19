#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>

// Arpeggiator / chord sequencer that sits in front of the synth (MIDI in -> MIDI out).
//  * modes: up, down, up/down variants, converge/diverge, random, as played, chord
//  * 16-step rhythm patterns (rest / normal / accent) that follow the host's beat grid
//  * swing, gate, octave range, latch, velocity humanise
//  * STUTTER: the step rate ramps up and/or down over a 1/2-4 bar cycle (melodic / deep-house style
//    roll: 1/8 -> 1/16 -> 1/32 ... and back), locked to the transport.
namespace ember::arp
{
    inline const juce::StringArray modeNames { "Up", "Down", "Up/Down", "Down/Up", "Up/Down Repeat", "Converge", "Diverge", "Random", "As Played", "Chord" };
    inline const juce::StringArray rateNames { "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T", "1/4" };
    inline constexpr float rateBeats[] { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f };

    // '.' rest, 'x' normal, 'X' accent; one character per arp step
    inline const juce::StringArray patternNames { "All Steps", "Rolling", "Offbeat", "Syncopated", "Gallop", "Skip", "Push", "Half Time", "Dotted" };
    inline constexpr const char* patternSteps[] {
        "xxxxxxxxxxxxxxxx",
        "XxxxXxxxXxxxXxxx",
        "..X...X...X...X.",
        "X..x..x.X..x..x.",
        "X.xxX.xxX.xxX.xx",
        "X.x.X.x.X.x.X.x.",
        "x.xXx.xXx.xXx.xX",
        "X...x...X...x...",
        "X..x..x..x..x..x",
    };

    inline const juce::StringArray stutterModeNames  { "Off", "Speed Up", "Slow Down", "Up & Down" };
    inline const juce::StringArray stutterLenNames   { "1/2 bar", "1 bar", "2 bars", "4 bars" };
    inline constexpr float stutterLenBeats[] { 2.0f, 4.0f, 8.0f, 16.0f };
    inline const juce::StringArray stutterCurveNames { "Smooth", "Stepped" };

    // 0..1 position within the stutter cycle -> how "fast" the roll is at that moment (0 = base rate, 1 = fastest)
    inline float stutterShape (int mode, float phase) noexcept
    {
        switch (mode)
        {
            case 1:  return phase;
            case 2:  return 1.0f - phase;
            case 3:  return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
            default: return 0.0f;
        }
    }

    // rate multiplier (1 = base rate .. 8 = three doublings faster at full depth)
    inline float stutterFactor (int mode, float depth, bool stepped, float phase) noexcept
    {
        const float octaves = depth * 3.0f * stutterShape (mode, phase);
        return std::exp2 (stepped ? std::floor (octaves + 0.5f) : octaves);
    }

    struct Settings
    {
        bool on = false, latch = false, stutterStepped = false;
        int mode = 0, rate = 2, octaves = 1, pattern = 0, stutterMode = 0, stutterLen = 1;
        float gate = 0.6f, swing = 0.0f, accent = 0.5f, humanize = 0.0f, stutterDepth = 0.5f;
    };

    struct Ptrs
    {
        std::atomic<float> *on, *mode, *rate, *octaves, *gate, *swing, *pattern, *accent, *humanize, *latch,
                           *stutterMode, *stutterDepth, *stutterLen, *stutterCurve;

        Settings read() const
        {
            Settings s;
            s.on = on->load() > 0.5f;
            s.latch = latch->load() > 0.5f;
            s.mode = juce::roundToInt (mode->load());
            s.rate = juce::roundToInt (rate->load());
            s.octaves = juce::roundToInt (octaves->load());
            s.gate = gate->load();
            s.swing = swing->load();
            s.pattern = juce::roundToInt (pattern->load());
            s.accent = accent->load();
            s.humanize = humanize->load();
            s.stutterMode = juce::roundToInt (stutterMode->load());
            s.stutterDepth = stutterDepth->load();
            s.stutterLen = juce::roundToInt (stutterLen->load());
            s.stutterStepped = stutterCurve->load() > 0.5f;
            return s;
        }
    };

    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p)
    {
        using namespace juce;
        auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
        {
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };
        auto flt = [&] (const char* id, const char* name, float lo, float hi, float def)
        {
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.001f), def));
        };

        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "arp_on", 1 }, "Arp On", false));
        choice ("arp_mode", "Arp Mode", modeNames, 0);
        choice ("arp_rate", "Arp Rate", rateNames, 2);
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "arp_octaves", 1 }, "Arp Octaves", 1, 4, 1));
        flt ("arp_gate", "Arp Gate", 0.05f, 1.0f, 0.6f);
        flt ("arp_swing", "Arp Swing", 0.0f, 0.5f, 0.0f);
        choice ("arp_pattern", "Arp Pattern", patternNames, 0);
        flt ("arp_accent", "Arp Accent", 0.0f, 1.0f, 0.5f);
        flt ("arp_humanize", "Arp Humanise", 0.0f, 1.0f, 0.0f);
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "arp_latch", 1 }, "Arp Latch", false));
        choice ("arp_stutter_mode", "Stutter Mode", stutterModeNames, 0);
        flt ("arp_stutter_depth", "Stutter Depth", 0.0f, 1.0f, 0.5f);
        choice ("arp_stutter_length", "Stutter Length", stutterLenNames, 1);
        choice ("arp_stutter_curve", "Stutter Curve", stutterCurveNames, 0);
    }

    inline Ptrs lookup (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&] (const char* id) { return apvts.getRawParameterValue (id); };
        return { get ("arp_on"), get ("arp_mode"), get ("arp_rate"), get ("arp_octaves"), get ("arp_gate"), get ("arp_swing"),
                 get ("arp_pattern"), get ("arp_accent"), get ("arp_humanize"), get ("arp_latch"), get ("arp_stutter_mode"),
                 get ("arp_stutter_depth"), get ("arp_stutter_length"), get ("arp_stutter_curve") };
    }

    class Arpeggiator
    {
    public:
        Arpeggiator() { out.ensureSize (8192); }

        void reset()
        {
            numPool = numSounding = 0;
            physical.reset();
            running = false;
            counter = patternStep = 0;
            wasOn = false;
            lastLatch = false;
        }

        // Rewrites `midi` in place. `ppq` < 0 means the transport is not running (the arp then free-runs on its own clock).
        void process (juce::MidiBuffer& midi, int numSamples, double sampleRate, double bpm, double ppq, const Settings& s)
        {
            out.clear();
            settings = s;
            blockStart = clock;
            bps = bpm / 60.0 / sampleRate;
            blockLen = numSamples;

            if (ppq >= 0.0)
            {
                if (running && std::abs (clock - ppq) > 0.05)
                    nextStep = ceilToGrid (ppq);
                clock = blockStart = ppq;
            }

            if (s.on && ! wasOn)
            {
                for (int n = 0; n < 128; ++n)   // notes that were played through while the arp was off
                    if (physical[(size_t) n])
                        out.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                if (numPool > 0)
                    startRunning (blockStart);
            }
            else if (! s.on && wasOn)
            {
                for (int i = 0; i < numSounding; ++i)
                    out.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) i].note), 0);
                numSounding = 0;
                running = false;
                for (int n = 0; n < 128; ++n)
                    if (physical[(size_t) n])
                        out.addEvent (juce::MidiMessage::noteOn (1, n, velocityOf (n)), 0);
                numPool = 0;
            }
            wasOn = s.on;

            if (! s.latch && lastLatch)   // latch released: keep only the keys that are physically down
            {
                int k = 0;
                for (int i = 0; i < numPool; ++i)
                    if (physical[(size_t) pool[(size_t) i].note])
                        pool[(size_t) k++] = pool[(size_t) i];
                numPool = k;
            }
            lastLatch = s.latch;

            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);

                if (! s.on)
                {
                    if (m.isNoteOn())       { physical.set ((size_t) m.getNoteNumber()); remember (m.getNoteNumber(), m.getFloatVelocity()); }
                    else if (m.isNoteOff()) { physical.reset ((size_t) m.getNoteNumber()); forget (m.getNoteNumber()); }
                    out.addEvent (m, pos);
                    continue;
                }

                advanceTo (blockStart + pos * bps);

                if (m.isNoteOn())
                {
                    if (s.latch && physical.none())
                        numPool = 0;
                    physical.set ((size_t) m.getNoteNumber());
                    remember (m.getNoteNumber(), m.getFloatVelocity());
                    if (! running)
                        startRunning (blockStart + pos * bps);
                }
                else if (m.isNoteOff())
                {
                    physical.reset ((size_t) m.getNoteNumber());
                    if (! s.latch)
                        forget (m.getNoteNumber());
                }
                else
                {
                    if (m.isAllNotesOff() || m.isAllSoundOff())
                    {
                        numPool = 0;
                        physical.reset();
                        for (int i = 0; i < numSounding; ++i)
                            out.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) i].note), pos);
                        numSounding = 0;
                    }
                    out.addEvent (m, pos);
                }
            }

            if (s.on)
                advanceTo (blockStart + numSamples * bps);

            clock = blockStart + numSamples * bps;
            midi.swapWith (out);
        }

        bool isRunning() const noexcept { return running; }

    private:
        struct Note { int note = 0; float vel = 0.8f; };
        struct Sounding { int note = 0; double offBeat = 0.0; };

        static constexpr size_t capacity = 32;

        double baseBeats() const noexcept { return (double) rateBeats[juce::jlimit (0, (int) std::size (rateBeats) - 1, settings.rate)]; }
        double ceilToGrid (double beat) const noexcept { return std::ceil (beat / baseBeats() - 1.0e-6) * baseBeats(); }

        float velocityOf (int note) const noexcept
        {
            for (int i = 0; i < numPool; ++i)
                if (pool[(size_t) i].note == note)
                    return pool[(size_t) i].vel;
            return 0.8f;
        }

        void remember (int note, float vel)
        {
            for (int i = 0; i < numPool; ++i)
                if (pool[(size_t) i].note == note) { pool[(size_t) i].vel = vel; return; }
            if (numPool < (int) capacity)
                pool[(size_t) numPool++] = { note, vel };
        }

        void forget (int note)
        {
            int k = 0;
            for (int i = 0; i < numPool; ++i)
                if (pool[(size_t) i].note != note)
                    pool[(size_t) k++] = pool[(size_t) i];
            numPool = k;
        }

        void startRunning (double beat)
        {
            running = true;
            counter = 0;
            nextStep = ceilToGrid (beat);
            patternStep = ((int) std::llround (nextStep / baseBeats()) % 16 + 16) % 16;
        }

        int sampleAt (double beat) const noexcept
        {
            return juce::jlimit (0, juce::jmax (0, blockLen - 1), (int) std::llround ((beat - blockStart) / bps));
        }

        void noteOn (int note, float vel, double beat, double offBeat)
        {
            for (int i = 0; i < numSounding; ++i)   // retrigger: end the previous instance first
                if (sounding[(size_t) i].note == note)
                {
                    out.addEvent (juce::MidiMessage::noteOff (1, note), sampleAt (beat));
                    sounding[(size_t) i] = sounding[(size_t) --numSounding];
                    break;
                }

            out.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.01f, 1.0f, vel)), sampleAt (beat));
            if (numSounding < (int) sounding.size())
                sounding[(size_t) numSounding++] = { note, offBeat };
        }

        void advanceTo (double tBeat)
        {
            for (;;)
            {
                int offIdx = -1;
                double offT = 1.0e18;
                for (int i = 0; i < numSounding; ++i)
                    if (sounding[(size_t) i].offBeat < offT) { offT = sounding[(size_t) i].offBeat; offIdx = i; }

                const bool offDue = offIdx >= 0 && offT <= tBeat;
                const bool stepDue = running && nextStep < tBeat;
                if (! offDue && ! stepDue)
                    break;

                if (offDue && (! stepDue || offT <= nextStep))
                {
                    out.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) offIdx].note), sampleAt (offT));
                    sounding[(size_t) offIdx] = sounding[(size_t) --numSounding];
                }
                else
                {
                    trigger (nextStep);
                }
            }
        }

        // order the notes for the current mode into `seq` (octaves expanded); returns the count
        int buildSequence()
        {
            std::array<Note, capacity> base = pool;
            const int n = numPool;
            if (settings.mode != 8)   // everything except "as played" runs low -> high
                std::sort (base.begin(), base.begin() + n, [] (const Note& a, const Note& b) { return a.note < b.note; });

            int total = 0;
            for (int o = 0; o < settings.octaves; ++o)
                for (int i = 0; i < n && total < (int) seq.size(); ++i)
                    if (base[(size_t) i].note + 12 * o <= 127)
                        seq[(size_t) total++] = { base[(size_t) i].note + 12 * o, base[(size_t) i].vel };
            return total;
        }

        static int converge (int k, int n) noexcept { return k % 2 == 0 ? k / 2 : n - 1 - k / 2; }

        int pickIndex (int n)
        {
            if (n <= 1)
                return 0;

            const int c = counter;
            switch (settings.mode)
            {
                case 0:  case 8: return c % n;
                case 1:  return n - 1 - c % n;
                case 2:  { const int period = 2 * n - 2, pos = c % period; return pos < n ? pos : period - pos; }
                case 3:  { const int period = 2 * n - 2, pos = c % period; return n - 1 - (pos < n ? pos : period - pos); }
                case 4:  { const int period = 2 * n, pos = c % period; return pos < n ? pos : period - 1 - pos; }
                case 5:  return converge (c % n, n);
                case 6:  return converge (n - 1 - c % n, n);
                case 7:
                {
                    int i = random.nextInt (n);
                    if (i == lastRandom)
                        i = (i + 1 + random.nextInt (n - 1)) % n;
                    return lastRandom = i;
                }
                default: return c % n;
            }
        }

        void trigger (double beat)
        {
            if (numPool == 0)
            {
                running = false;
                return;
            }

            const double base = baseBeats();
            const bool stutter = settings.stutterMode != 0;
            const double cycle = (double) stutterLenBeats[juce::jlimit (0, 3, settings.stutterLen)];
            const float phase = (float) (std::fmod (beat, cycle) / cycle);
            const float shape = stutterShape (settings.stutterMode, phase);
            const float factor = stutter ? stutterFactor (settings.stutterMode, settings.stutterDepth, settings.stutterStepped, phase) : 1.0f;
            const bool rhythmic = factor < 1.5f;   // rolling subdivisions are always played in full

            double dur = base / (double) factor;
            if (rhythmic && settings.swing > 0.0f)
                dur *= (patternStep % 2 == 0) ? 1.0 + (double) settings.swing : 1.0 - (double) settings.swing;

            const char step = rhythmic ? patternSteps[juce::jlimit (0, (int) std::size (patternSteps) - 1, settings.pattern)][patternStep % 16] : 'x';
            if (step != '.')
            {
                float level = step == 'X' ? 1.0f : 1.0f - 0.5f * settings.accent;
                if (stutter)
                    level *= 0.7f + 0.3f * shape;   // the roll swells as it speeds up
                const double offBeat = beat + dur * (double) settings.gate;

                auto vel = [&] (float v)
                {
                    const float jitter = settings.humanize > 0.0f ? (random.nextFloat() * 2.0f - 1.0f) * 0.25f * settings.humanize : 0.0f;
                    return v * level * (1.0f + jitter);
                };

                const int n = buildSequence();
                if (n > 0)
                {
                    if (settings.mode == 9)
                    {
                        const int shift = settings.octaves > 1 ? 12 * (counter % settings.octaves) : 0;
                        for (int i = 0; i < numPool; ++i)
                            if (pool[(size_t) i].note + shift <= 127)
                                noteOn (pool[(size_t) i].note + shift, vel (pool[(size_t) i].vel), beat, offBeat);
                    }
                    else
                    {
                        const auto& note = seq[(size_t) pickIndex (n)];
                        noteOn (note.note, vel (note.vel), beat, offBeat);
                    }
                }
                ++counter;
            }

            ++patternStep;
            nextStep = beat + dur;
        }

        juce::MidiBuffer out;
        juce::Random random { 0x4a5b };
        Settings settings;
        std::array<Note, capacity> pool {};
        std::array<Note, capacity * 4> seq {};
        std::array<Sounding, 64> sounding {};
        std::bitset<128> physical;
        int numPool = 0, numSounding = 0, counter = 0, patternStep = 0, lastRandom = -1, blockLen = 0;
        double clock = 0.0, blockStart = 0.0, bps = 0.0, nextStep = 0.0;
        bool running = false, wasOn = false, lastLatch = false;
    };
}
