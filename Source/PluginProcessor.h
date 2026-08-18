#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "LfoShape.h"
#include "Modulation.h"
#include "WavetableBank.h"

class EmberlineProcessor final : public juce::AudioProcessor
{
public:
    EmberlineProcessor();
    ~EmberlineProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Maps a modulation source onto a destination (index into mod::destinations()) using the first
    // free matrix slot. Message thread only. Returns false if the matrix is full.
    bool assignModulation (int source, int destination);

    juce::AudioProcessorValueTreeState apvts;
    ember::mod::ModPtrs modPtrs {};   // cached raw parameter pointers for the editor's mod displays
    ember::mod::LfoShapes lfoShapes;
    ember::mod::ModContext modCtx { lfoShapes };

private:
    std::atomic<float>* volumeParam = nullptr;

    double currentSampleRate = 44100.0;
    std::array<std::atomic<float>*, ember::mod::numLfos> lfoRate {}, lfoMode {};

    ember::WavetableBank bank;   // immutable after construction; shared by all voices
    juce::Synthesiser synth;
    juce::SmoothedValue<float> gain;

    static constexpr int numVoices = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineProcessor)
};
