#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace ember::params
{
    inline constexpr auto masterVolume = "master_volume";
    inline constexpr auto filterCutoff = "filter_cutoff";

    inline constexpr int numOscs = 3;

    inline juce::String osc (int index, const char* name) { return "osc" + juce::String (index + 1) + "_" + name; }

    inline const juce::StringArray warpModeNames { "Off", "Formant", "Sync", "Bend", "Phase Dist" };

    struct OscParamPtrs
    {
        std::atomic<float> *level, *frame, *warpMode, *warpAmt, *unison, *detune, *spread, *semi, *fine;
    };

    inline OscParamPtrs lookupOsc (juce::AudioProcessorValueTreeState& apvts, int i)
    {
        auto get = [&] (const char* n) { return apvts.getRawParameterValue (osc (i, n)); };
        return { get ("level"), get ("frame"), get ("warp_mode"), get ("warp_amt"),
                 get ("unison"), get ("detune"), get ("spread"), get ("semi"), get ("fine") };
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        using Attr = AudioParameterFloatAttributes;

        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { masterVolume, 1 }, "Master Volume",
            NormalisableRange<float> (-60.0f, 6.0f, 0.01f, 2.5f), -6.0f, Attr().withLabel ("dB")));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { filterCutoff, 1 }, "Filter Cutoff",
            NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.25f), 8000.0f, Attr().withLabel ("Hz")));

        for (int i = 0; i < numOscs; ++i)
        {
            const auto n = "Osc " + String (i + 1) + " ";
            const bool first = (i == 0);

            auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
            {
                p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { osc (i, id), 1 }, n + name, r, def, Attr().withLabel (unit)));
            };

            addFloat ("level",    "Level",       { 0.0f, 1.0f, 0.001f }, first ? 0.8f : 0.0f);
            addFloat ("frame",    "Wave Frame",  { 0.0f, 1.0f, 0.001f }, first ? 0.5f : 0.0f);
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { osc (i, "warp_mode"), 1 }, n + "Warp Mode", warpModeNames, 0));
            addFloat ("warp_amt", "Warp Amount", { 0.0f, 1.0f, 0.001f }, 0.0f);
            p.push_back (std::make_unique<AudioParameterInt> (ParameterID { osc (i, "unison"), 1 }, n + "Unison Voices", 1, 16, 1));
            addFloat ("detune",   "Unison Detune", { 0.0f, 100.0f, 0.1f, 0.5f }, 15.0f, "ct");
            addFloat ("spread",   "Unison Spread", { 0.0f, 1.0f, 0.001f }, 0.5f);
            p.push_back (std::make_unique<AudioParameterInt> (ParameterID { osc (i, "semi"), 1 }, n + "Semitones", -24, 24, 0));
            addFloat ("fine",     "Fine Tune",   { -100.0f, 100.0f, 0.1f }, 0.0f, "ct");
        }

        return { p.begin(), p.end() };
    }
}
