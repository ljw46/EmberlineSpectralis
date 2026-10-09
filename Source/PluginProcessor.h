#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Chaos.h"
#include "Fx.h"
#include "LfoShape.h"
#include "Modulation.h"
#include "Presets.h"
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

    // the host's program list = the factory presets
    int getNumPrograms() override { return juce::jmax (1, presetLibrary.numFactory()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override
    {
        return juce::isPositiveAndBelow (index, presetLibrary.numFactory()) ? presetLibrary.all()[(size_t) index].name : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Maps a modulation source onto a destination (index into mod::destinations()) using the first
    // free matrix slot. Message thread only. Returns false if the matrix is full.
    bool assignModulation (int source, int destination);

    // Presets (JSON, see docs/PRESETS.md). Message thread only.
    bool loadPreset (int libraryIndex);
    bool loadPresetByName (const juce::String& name);
    bool saveUserPreset (const juce::String& name, const juce::String& category, const juce::String& description = {});
    juce::String getCurrentPresetName() const { return apvts.state["presetName"].toString(); }
    juce::String getMacroNames() const { return apvts.state["macroNames"].toString(); }   // "a|b|c|d", from the preset's macro_names
    ember::presets::Library presetLibrary;
    ember::presets::Result lastPresetResult;

    ember::WavetableBank& getWavetableBank() const noexcept { return bank; }

    juce::AudioProcessorValueTreeState apvts;
    ember::mod::ModPtrs modPtrs {};   // cached raw parameter pointers for the editor's mod displays
    ember::mod::LfoShapes lfoShapes;
    ember::mod::ModContext modCtx { lfoShapes };
    int currentProgram = 0;

private:
    std::atomic<float>* volumeParam = nullptr;

    double currentSampleRate = 44100.0;
    std::array<std::atomic<float>*, ember::mod::numLfos> lfoRate {}, lfoMode {}, lfoSync {};

    ember::ChaosEngine chaos;
    ember::fx::Chain fxChain;
    ember::fx::Ptrs fxPtrs {};
    ember::WavetableBank& bank = ember::WavetableBank::shared();   // one per process; shared by every instance and voice
    juce::Synthesiser synth;
    juce::SmoothedValue<float> gain;

    static constexpr int numVoices = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineProcessor)
};
