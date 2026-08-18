#pragma once

#include "LfoShape.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace ember::mod
{
    inline constexpr int numEnvs = 3, numMacros = 4, numSlots = 12, oscCount = 3;

    // Source indices (0 = none). The matrix stores these as choice indices.
    inline constexpr int srcNone = 0, srcLfo = 1, srcEnv = 4, srcMacro = 7, numSources = 11;

    inline juce::StringArray sourceNames()
    {
        return { "None", "LFO 1", "LFO 2", "LFO 3", "ENV 1", "ENV 2", "ENV 3", "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    }

    enum class SourceKind { lfo, env, macro };
    inline SourceKind sourceKind (int src) { return src < srcEnv ? SourceKind::lfo : src < srcMacro ? SourceKind::env : SourceKind::macro; }
    inline bool isBipolar (int src) { return sourceKind (src) == SourceKind::lfo; }

    // Destination indices (matrix choice index = dest + 1; 0 = none).
    inline constexpr int oscDestStride = 6;   // frame, warp_amt, level, detune, spread, fine
    inline constexpr int filterDestBase = oscCount * oscDestStride;
    inline constexpr int subDest = filterDestBase + 6, noiseDest = subDest + 1, numDests = noiseDest + 1;

    inline constexpr int oscDest (int osc, int k) { return osc * oscDestStride + k; }
    inline constexpr int filterDest (int filter, int k) { return filterDestBase + filter * 3 + k; }

    struct DestInfo { juce::String paramId, label; };

    inline const std::vector<DestInfo>& destinations()
    {
        static const auto list = []
        {
            std::vector<DestInfo> d;
            const char* oscIds[]    { "frame", "warp_amt", "level", "detune", "spread", "fine" };
            const char* oscLabels[] { "Frame", "Warp", "Level", "Detune", "Spread", "Fine" };
            for (int o = 0; o < oscCount; ++o)
                for (int k = 0; k < oscDestStride; ++k)
                    d.push_back ({ "osc" + juce::String (o + 1) + "_" + oscIds[k], "Osc" + juce::String (o + 1) + " " + oscLabels[k] });

            const char* fIds[]    { "cutoff", "res", "drive" };
            const char* fLabels[] { "Cutoff", "Res", "Drive" };
            for (int f = 0; f < 2; ++f)
                for (int k = 0; k < 3; ++k)
                    d.push_back ({ "filt" + juce::String (f + 1) + "_" + fIds[k], "Filt" + juce::String (f + 1) + " " + fLabels[k] });

            d.push_back ({ "sub_level", "Sub Level" });
            d.push_back ({ "noise_level", "Noise Level" });
            return d;
        }();
        return list;
    }

    inline int destForParam (const juce::String& id)
    {
        const auto& d = destinations();
        for (size_t i = 0; i < d.size(); ++i)
            if (d[i].paramId == id)
                return (int) i;
        return -1;
    }

    inline juce::StringArray destChoiceNames()
    {
        juce::StringArray names { "None" };
        for (auto& d : destinations())
            names.add (d.label);
        return names;
    }

    inline juce::String slotId (int slot, const char* what) { return "mod" + juce::String (slot + 1) + "_" + what; }

    struct SlotPtrs { std::atomic<float> *src, *dst, *depth; };
    struct LfoPtrs  { std::atomic<float> *rate, *mode; };
    struct EnvPtrs  { std::atomic<float> *attack, *decay, *sustain, *release, *aCurve, *dCurve; };

    struct ModPtrs
    {
        std::array<SlotPtrs, numSlots> slots;
        std::array<LfoPtrs, numLfos> lfo;
        std::array<EnvPtrs, numEnvs> env;
        std::array<std::atomic<float>*, numMacros> macro;
        std::array<std::atomic<float>*, numDests> destBase;
        std::array<juce::NormalisableRange<float>, numDests> destRange;
    };

    inline const juce::StringArray lfoModeNames { "Retrigger", "Free" };

    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p)
    {
        using namespace juce;

        for (int i = 0; i < numLfos; ++i)
        {
            const auto n = "LFO " + String (i + 1) + " ";
            const auto id = "lfo" + String (i + 1) + "_";
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "rate", 1 }, n + "Rate",
                NormalisableRange<float> (0.01f, 30.0f, 0.001f, 0.35f), 2.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id + "mode", 1 }, n + "Mode", lfoModeNames, 0));
        }

        for (int i = 0; i < numEnvs; ++i)
        {
            const auto n = "Env " + String (i + 1) + " ";
            const auto id = "env" + String (i + 1) + "_";
            const bool amp = (i == 0);
            const NormalisableRange<float> time (0.001f, 10.0f, 0.001f, 0.3f);
            const auto sec = AudioParameterFloatAttributes().withLabel ("s");

            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "attack", 1 },  n + "Attack",  time, amp ? 0.005f : 0.01f, sec));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "decay", 1 },   n + "Decay",   time, amp ? 0.3f : 0.4f, sec));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "sustain", 1 }, n + "Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), amp ? 0.7f : 0.0f));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "release", 1 }, n + "Release", time, amp ? 0.25f : 0.3f, sec));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "acurve", 1 },  n + "Attack Curve", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "dcurve", 1 },  n + "Decay Curve",  NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.4f));
        }

        for (int i = 0; i < numMacros; ++i)
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "macro" + String (i + 1), 1 }, "Macro " + String (i + 1),
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

        const auto srcs = sourceNames();
        const auto dsts = destChoiceNames();
        for (int s = 0; s < numSlots; ++s)
        {
            const auto n = "Mod " + String (s + 1) + " ";
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { slotId (s, "src"), 1 }, n + "Source", srcs, 0));
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { slotId (s, "dst"), 1 }, n + "Destination", dsts, 0));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { slotId (s, "depth"), 1 }, n + "Depth",
                NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.5f));
        }
    }

    inline ModPtrs lookup (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&] (const juce::String& id) { return apvts.getRawParameterValue (id); };
        ModPtrs m {};

        for (int s = 0; s < numSlots; ++s)
            m.slots[(size_t) s] = { get (slotId (s, "src")), get (slotId (s, "dst")), get (slotId (s, "depth")) };

        for (int i = 0; i < numLfos; ++i)
        {
            const auto id = "lfo" + juce::String (i + 1) + "_";
            m.lfo[(size_t) i] = { get (id + "rate"), get (id + "mode") };
        }

        for (int i = 0; i < numEnvs; ++i)
        {
            const auto id = "env" + juce::String (i + 1) + "_";
            m.env[(size_t) i] = { get (id + "attack"), get (id + "decay"), get (id + "sustain"),
                                  get (id + "release"), get (id + "acurve"), get (id + "dcurve") };
        }

        for (int i = 0; i < numMacros; ++i)
            m.macro[(size_t) i] = get ("macro" + juce::String (i + 1));

        const auto& d = destinations();
        for (size_t i = 0; i < d.size(); ++i)
        {
            m.destBase[i] = get (d[i].paramId);
            m.destRange[i] = apvts.getParameterRange (d[i].paramId);
        }
        return m;
    }

    // State shared between the processor, its voices and the editor.
    struct ModContext
    {
        explicit ModContext (LfoShapes& s) : shapes (s) {}

        LfoShapes& shapes;
        std::array<double, numLfos> blockPhase {};                     // free-running LFO phase at the start of the current block (audio thread)
        std::array<std::atomic<float>, numLfos> displayPhase {};       // for the editor playhead
        std::array<std::atomic<float>, numDests> liveOffset {};        // newest voice's modulation, for the knob arcs
        std::atomic<std::uint64_t> stampCounter { 0 }, newestStamp { 0 };
    };
}
