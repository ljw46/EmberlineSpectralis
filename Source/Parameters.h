#pragma once

#include "Arp.h"
#include "Fx.h"
#include "Modulation.h"
#include "WavetableBank.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace ember::params
{
    inline constexpr auto masterVolume = "master_volume";

    inline const juce::StringArray filterTypeNames { "Off", "Lowpass", "Highpass", "Bandpass", "Formant", "Notch", "Ladder" };
    inline const juce::StringArray filterSlopeNames { "12 dB", "24 dB" };
    inline const juce::StringArray noiseTypeNames  { "White", "Pink", "Crackle" };
    inline const juce::StringArray subWaveNames    { "Sine", "Triangle" };
    inline const juce::StringArray subOctNames     { "-1 Oct", "-2 Oct" };
    inline const juce::StringArray routingNames    { "Serial", "Parallel" };

    inline juce::String filt (int index, const char* name) { return "filt" + juce::String (index + 1) + "_" + name; }

    inline constexpr int numOscs = 3;
    static_assert (numOscs == mod::oscCount);

    inline juce::String osc (int index, const char* name) { return "osc" + juce::String (index + 1) + "_" + name; }

    inline const juce::StringArray warpModeNames { "Off", "Formant", "Sync", "Bend", "Phase Dist" };

    struct OscParamPtrs
    {
        std::atomic<float> *on, *level, *table, *frame, *warpMode, *warpAmt, *unison, *detune, *spread, *semi, *fine, *pan;
    };

    struct FilterParamPtrs { std::atomic<float> *type, *cutoff, *res, *drive, *slope; };

    struct SynthParamPtrs
    {
        std::array<OscParamPtrs, numOscs> osc;
        std::atomic<float> *subLevel, *subWave, *subOct, *noiseLevel, *noiseType;
        std::array<FilterParamPtrs, 2> filter;
        std::atomic<float>* routing;
        std::atomic<float> *drift, *warmth;   // analogue character (per-voice drift, saturation + high-end softening)
        mod::ModPtrs mod;
    };

    inline OscParamPtrs lookupOsc (juce::AudioProcessorValueTreeState& apvts, int i)
    {
        auto get = [&] (const char* n) { return apvts.getRawParameterValue (osc (i, n)); };
        return { get ("on"), get ("level"), get ("table"), get ("frame"), get ("warp_mode"), get ("warp_amt"),
                 get ("unison"), get ("detune"), get ("spread"), get ("semi"), get ("fine"), get ("pan") };
    }

    inline SynthParamPtrs lookupAll (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&] (const juce::String& id) { return apvts.getRawParameterValue (id); };
        auto filterPtrs = [&] (int i) -> FilterParamPtrs
        {
            return { get (filt (i, "type")), get (filt (i, "cutoff")), get (filt (i, "res")), get (filt (i, "drive")), get (filt (i, "slope")) };
        };

        return { { lookupOsc (apvts, 0), lookupOsc (apvts, 1), lookupOsc (apvts, 2) },
                 get ("sub_level"), get ("sub_wave"), get ("sub_oct"), get ("noise_level"), get ("noise_type"),
                 { filterPtrs (0), filterPtrs (1) }, get ("filt_routing"), get ("analog_drift"), get ("analog_warmth"), mod::lookup (apvts) };
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        using Attr = AudioParameterFloatAttributes;

        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { masterVolume, 1 }, "Master Volume",
            NormalisableRange<float> (-60.0f, 6.0f, 0.01f, 2.5f), -6.0f, Attr().withLabel ("dB")));

        auto choice = [&] (const String& id, const String& name, const StringArray& items, int def = 0)
        {
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };
        auto level = [&] (const String& id, const String& name)
        {
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
        };

        level ("sub_level", "Sub Level");
        choice ("sub_wave", "Sub Wave", subWaveNames);
        choice ("sub_oct", "Sub Octave", subOctNames);
        level ("noise_level", "Noise Level");
        choice ("noise_type", "Noise Type", noiseTypeNames);
        choice ("filt_routing", "Filter Routing", routingNames);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "analog_drift", 1 }, "Analog Drift", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "analog_warmth", 1 }, "Analog Warmth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

        for (int i = 0; i < 2; ++i)
        {
            const auto n = "Filter " + String (i + 1) + " ";
            choice (filt (i, "type"), n + "Type", filterTypeNames, i == 0 ? 1 : 0);
            choice (filt (i, "slope"), n + "Slope", filterSlopeNames);
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { filt (i, "cutoff"), 1 }, n + "Cutoff",
                NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.25f), i == 0 ? 8000.0f : 300.0f, Attr().withLabel ("Hz")));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { filt (i, "res"), 1 }, n + "Resonance",
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f));
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { filt (i, "drive"), 1 }, n + "Drive",
                NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
        }

        for (int i = 0; i < numOscs; ++i)
        {
            const auto n = "Osc " + String (i + 1) + " ";
            const bool first = (i == 0);

            auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
            {
                p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { osc (i, id), 1 }, n + name, r, def, Attr().withLabel (unit)));
            };

            p.push_back (std::make_unique<AudioParameterBool> (ParameterID { osc (i, "on"), 1 }, n + "On", first));
            addFloat ("level",    "Level",       { 0.0f, 1.0f, 0.001f }, 0.8f);
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { osc (i, "table"), 1 }, n + "Wavetable",
                                                                  WavetableBank::shared().paramChoiceNames(), 0));
            addFloat ("frame",    "Wave Frame",  { 0.0f, 1.0f, 0.001f }, first ? 0.5f : 0.0f);
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { osc (i, "warp_mode"), 1 }, n + "Warp Mode", warpModeNames, 0));
            addFloat ("warp_amt", "Warp Amount", { 0.0f, 1.0f, 0.001f }, 0.0f);
            p.push_back (std::make_unique<AudioParameterInt> (ParameterID { osc (i, "unison"), 1 }, n + "Unison Voices", 1, 16, 1));
            addFloat ("detune",   "Unison Detune", { 0.0f, 100.0f, 0.1f, 0.5f }, 15.0f, "ct");
            addFloat ("spread",   "Unison Spread", { 0.0f, 1.0f, 0.001f }, 0.5f);
            p.push_back (std::make_unique<AudioParameterInt> (ParameterID { osc (i, "semi"), 1 }, n + "Semitones", -24, 24, 0));
            addFloat ("fine",     "Fine Tune",   { -100.0f, 100.0f, 0.1f }, 0.0f, "ct");
            addFloat ("pan",      "Pan",         { -1.0f, 1.0f, 0.001f }, 0.0f);
        }

        mod::addParameters (p);
        fx::addParameters (p);
        arp::addParameters (p);

        return { p.begin(), p.end() };
    }
}
