#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

EmberlineProcessor::EmberlineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", ember::params::createLayout())
{
    volumeParam = apvts.getRawParameterValue (ember::params::masterVolume);

    const auto ptrs = ember::params::lookupAll (apvts);

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new ember::SynthVoice (bank, ptrs, (uint32_t) (i + 1)));
    synth.addSound (new ember::SynthSound());
}

void EmberlineProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    gain.reset (sampleRate, 0.02);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam->load()));
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

    const auto n = buffer.getNumSamples();
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const auto g = gain.getNextValue();
        l[i] *= g;
        r[i] *= g;
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
