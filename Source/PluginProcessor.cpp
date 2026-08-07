#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

EmberlineProcessor::EmberlineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", ember::params::createLayout())
{
    cutoffParam = apvts.getRawParameterValue (ember::params::filterCutoff);
    volumeParam = apvts.getRawParameterValue (ember::params::masterVolume);

    std::array<ember::params::OscParamPtrs, ember::params::numOscs> oscPtrs {
        ember::params::lookupOsc (apvts, 0), ember::params::lookupOsc (apvts, 1), ember::params::lookupOsc (apvts, 2) };

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new ember::SynthVoice (bank, oscPtrs));
    synth.addSound (new ember::SynthSound());
}

void EmberlineProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    filter.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();

    gain.reset (sampleRate, 0.02);
    cutoff.reset (sampleRate, 0.02);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam->load()));
    cutoff.setCurrentAndTargetValue (cutoffParam->load());
}

bool EmberlineProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EmberlineProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    gain.setTargetValue (juce::Decibels::decibelsToGain (volumeParam->load()));
    cutoff.setTargetValue (cutoffParam->load());

    // Per-sample cutoff smoothing (filter coefficients update each sample)
    const auto n = buffer.getNumSamples();
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        filter.setCutoffFrequency (cutoff.getNextValue());
        const auto g = gain.getNextValue();
        l[i] = filter.processSample (0, l[i]) * g;
        r[i] = filter.processSample (1, r[i]) * g;
    }
}

juce::AudioProcessorEditor* EmberlineProcessor::createEditor() { return new EmberlineEditor (*this); }

void EmberlineProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void EmberlineProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EmberlineProcessor(); }
