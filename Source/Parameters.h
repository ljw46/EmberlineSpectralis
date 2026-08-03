#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ember::params
{
    inline constexpr auto masterVolume = "master_volume";
    inline constexpr auto oscMorph     = "osc_morph";
    inline constexpr auto filterCutoff = "filter_cutoff";

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;

        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID { masterVolume, 1 }, "Master Volume",
            NormalisableRange<float>(-60.0f, 6.0f, 0.01f, 2.5f), -6.0f,
            AudioParameterFloatAttributes().withLabel("dB")));

        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID { oscMorph, 1 }, "Osc Morph",
            NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID { filterCutoff, 1 }, "Filter Cutoff",
            NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.25f), 8000.0f,
            AudioParameterFloatAttributes().withLabel("Hz")));

        return { p.begin(), p.end() };
    }
}
