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
        lfoSync[i] = ptrs.mod.lfo[i].sync;
    }

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new ember::SynthVoice (bank, ptrs, modCtx, (uint32_t) (i + 1)));
    synth.addSound (new ember::SynthSound());

    presetLibrary.scanFactory();
    presetLibrary.scanUser();
    loadPresetByName ("Warm Keys");   // a musical starting point instead of a bare saw
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

    // transport: host tempo if there is one (standalone falls back to the tempo knob)
    {
        double bpm = (double) modPtrs.tempo->load();
        double ppq = -1.0;
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = *b;
                if (pos->getIsPlaying())
                    if (auto q = pos->getPpqPosition()) ppq = *q;
            }
        modCtx.bpm = juce::jmax (20.0, bpm);
        modCtx.hostPpq = ppq;

        // free-running, tempo-synced LFOs lock to the host's musical position
        for (size_t i = 0; i < lfoRate.size(); ++i)
        {
            const int sync = juce::roundToInt (lfoSync[i]->load());
            if (sync > 0 && ppq >= 0.0 && lfoMode[i]->load() > 0.5f)
            {
                const double cycles = ppq / (double) ember::tempo::beats (sync);
                modCtx.blockPhase[i] = cycles - std::floor (cycles);
            }
        }
    }

    // channel state the voices read as modulation sources / pitch bend (block-accurate: the last value in the block wins)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isPitchWheel())                                   modCtx.pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isController() && m.getControllerNumber() == 1)  modCtx.modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isChannelPressure())                         modCtx.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isAftertouch())                              modCtx.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff())        modCtx.aftertouch = 0.0f;
    }

    modCtx.blockLength = buffer.getNumSamples();
    chaos.process (buffer.getNumSamples(), modPtrs.chaosRate->load(), modPtrs.fractalRate->load(), modCtx.chaosPrev, modCtx.chaosNext);
    {
        const int head = modCtx.trailHead.load (std::memory_order_relaxed);
        modCtx.trailX[(size_t) head].store (chaos.displayX(), std::memory_order_relaxed);
        modCtx.trailY[(size_t) head].store (chaos.displayY(), std::memory_order_relaxed);
        modCtx.trailZ[(size_t) head].store (chaos.displayZ(), std::memory_order_relaxed);
        modCtx.trailHead.store ((head + 1) % ember::mod::ModContext::trailSize, std::memory_order_relaxed);
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // advance the free-running LFO clocks for the next block
    for (size_t i = 0; i < lfoRate.size(); ++i)
    {
        auto& ph = modCtx.blockPhase[i];
        ph += (double) buffer.getNumSamples() * (double) ember::tempo::rateHz (juce::roundToInt (lfoSync[i]->load()), lfoRate[i]->load(), modCtx.bpm) / currentSampleRate;
        ph -= std::floor (ph);
        if (lfoMode[i]->load() > 0.5f)
            modCtx.displayPhase[i].store ((float) ph, std::memory_order_relaxed);
    }

    // modulation of the global FX parameters: only sources that exist outside a single voice can drive them
    modCtx.fxOffset.fill (0.0f);
    for (const auto& slot : modPtrs.slots)
    {
        const int src = juce::roundToInt (slot.src->load());
        const int dst = juce::roundToInt (slot.dst->load()) - 1 - ember::mod::fxDestBase;
        if (src <= 0 || dst < 0 || dst >= ember::mod::numFxDests)
            continue;

        float v = 0.0f;
        if (src >= ember::mod::srcLfo && src < ember::mod::srcLfo + ember::mod::numLfos)
            v = lfoShapes.read (src - ember::mod::srcLfo, modCtx.blockPhase[(size_t) (src - ember::mod::srcLfo)]);
        else if (src >= ember::mod::srcMacro && src < ember::mod::srcMacro + ember::mod::numMacros)
            v = modPtrs.macro[(size_t) (src - ember::mod::srcMacro)]->load();
        else if (src >= ember::mod::srcChaos && src < ember::mod::srcChaos + ember::mod::numChaos)
            v = modCtx.chaosNext[(size_t) (src - ember::mod::srcChaos)];
        else if (src == ember::mod::srcPerform + 2)
            v = modCtx.modWheel;
        else if (src == ember::mod::srcPerform + 3)
            v = modCtx.aftertouch;
        else
            continue;   // envelopes, velocity, key and random are per voice

        modCtx.fxOffset[(size_t) dst] += slot.depth->load()
                                          * ember::mod::shapeSource (v, src, slot.bipolar->load() > 0.5f, slot.curve->load());
    }
    for (size_t i = 0; i < modCtx.fxOffset.size(); ++i)
        modCtx.liveOffset[(size_t) ember::mod::fxDestBase + i].store (modCtx.fxOffset[i], std::memory_order_relaxed);

    fxChain.process (buffer, fxPtrs, modCtx.bpm, modCtx.fxOffset.data());

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

bool EmberlineProcessor::loadPreset (int index)
{
    const auto& all = presetLibrary.all();
    if (! juce::isPositiveAndBelow (index, (int) all.size()))
        return false;

    juce::var v;
    juce::String error;
    if (! ember::presets::parse (all[(size_t) index].json, v, error))
        return false;

    lastPresetResult = ember::presets::apply (*this, v);
    if (index < presetLibrary.numFactory())
        currentProgram = index;
    return lastPresetResult.ok;
}

bool EmberlineProcessor::loadPresetByName (const juce::String& name)
{
    const int i = presetLibrary.indexOf (name);
    return i >= 0 && loadPreset (i);
}

bool EmberlineProcessor::saveUserPreset (const juce::String& name, const juce::String& category, const juce::String& description)
{
    const auto dir = ember::presets::Library::userDirectory();
    if (! dir.createDirectory().wasOk())
        return false;

    const auto file = dir.getChildFile (ember::presets::safeFileName (name) + ".json");
    const auto text = ember::presets::toJson (ember::presets::capture (*this, name, category.isEmpty() ? "User" : category, description));
    if (! file.replaceWithText (text, false, false, "\n"))   // LF everywhere, same on every platform
        return false;

    apvts.state.setProperty ("presetName", name, nullptr);
    apvts.state.setProperty ("presetCategory", category, nullptr);
    presetLibrary.scanUser();
    return true;
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
    set (ember::mod::slotId (freeSlot, "curve"), 0.0f);
    set (ember::mod::slotId (freeSlot, "bipolar"), ember::mod::isBipolar (source) ? 1.0f : 0.0f);
    set (ember::mod::slotId (freeSlot, "dst"), (float) (destination + 1));
    set (ember::mod::slotId (freeSlot, "src"), (float) source);
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EmberlineProcessor(); }
