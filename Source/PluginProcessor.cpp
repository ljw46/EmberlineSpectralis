#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

EmberlineProcessor::EmberlineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", ember::params::createLayout())
{
    volumeParam = apvts.getRawParameterValue (ember::params::masterVolume);

    const auto ptrs = ember::params::lookupAll (apvts);
    modPtrs = ptrs.mod;
    fxPtrs = ember::fx::lookup (apvts);

    for (size_t i = 0; i < lfoRate.size(); ++i)
    {
        lfoRate[i] = ptrs.mod.lfo[i].rate;
        lfoMode[i] = ptrs.mod.lfo[i].mode;
    }

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new ember::SynthVoice (bank, ptrs, modCtx, (uint32_t) (i + 1)));
    synth.addSound (new ember::SynthSound());
}

void EmberlineProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    modCtx.blockPhase.fill (0.0);
    chaos.prepare (sampleRate);
    fxChain.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (fxChain.getLatency());

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

    modCtx.blockLength = buffer.getNumSamples();
    chaos.process (buffer.getNumSamples(), modPtrs.chaosRate->load(), modPtrs.fractalRate->load(), modCtx.chaosPrev, modCtx.chaosNext);
    {
        const int head = modCtx.trailHead.load (std::memory_order_relaxed);
        modCtx.trailX[(size_t) head].store (chaos.displayX(), std::memory_order_relaxed);
        modCtx.trailZ[(size_t) head].store (chaos.displayZ(), std::memory_order_relaxed);
        modCtx.trailHead.store ((head + 1) % ember::mod::ModContext::trailSize, std::memory_order_relaxed);
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // advance the free-running LFO clocks for the next block
    for (size_t i = 0; i < lfoRate.size(); ++i)
    {
        auto& ph = modCtx.blockPhase[i];
        ph += (double) buffer.getNumSamples() * (double) lfoRate[i]->load() / currentSampleRate;
        ph -= std::floor (ph);
        if (lfoMode[i]->load() > 0.5f)
            modCtx.displayPhase[i].store ((float) ph, std::memory_order_relaxed);
    }

    fxChain.process (buffer, fxPtrs);

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
    auto state = apvts.copyState();
    for (int i = 0; i < ember::mod::numLfos; ++i)
        state.setProperty ("lfo" + juce::String (i + 1) + "_shape", lfoShapes.toString (i), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void EmberlineProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            for (int i = 0; i < ember::mod::numLfos; ++i)
                if (state.hasProperty ("lfo" + juce::String (i + 1) + "_shape"))
                    lfoShapes.fromString (i, state["lfo" + juce::String (i + 1) + "_shape"].toString());

            apvts.replaceState (state);
        }
}

bool EmberlineProcessor::assignModulation (int source, int destination)
{
    int freeSlot = -1;
    for (int s = 0; s < ember::mod::numSlots; ++s)
    {
        const int src = juce::roundToInt (apvts.getRawParameterValue (ember::mod::slotId (s, "src"))->load());
        const int dst = juce::roundToInt (apvts.getRawParameterValue (ember::mod::slotId (s, "dst"))->load()) - 1;
        if (src == source && dst == destination)
            return true;   // already routed
        if (src == 0 && freeSlot < 0)
            freeSlot = s;
    }

    if (freeSlot < 0)
        return false;

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    set (ember::mod::slotId (freeSlot, "depth"), 0.5f);
    set (ember::mod::slotId (freeSlot, "dst"), (float) (destination + 1));
    set (ember::mod::slotId (freeSlot, "src"), (float) source);
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EmberlineProcessor(); }
