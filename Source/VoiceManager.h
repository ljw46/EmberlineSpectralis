#pragma once

#include "SynthVoice.h"
#include <vector>

namespace ember
{
    // Poly / Mono / Legato note handling on top of juce::Synthesiser.
    //  Poly:   normal voice allocation (glide still works: each new note slides from the previous note's pitch).
    //  Mono:   one voice, last note priority; every new note retriggers the envelopes (and glides if enabled).
    //  Legato: one voice, last note priority; a note played while another is held just changes pitch (no retrigger).
    // Letting go of the top note while an older one is still held returns to the older one.
    class VoiceManager final : public juce::Synthesiser
    {
    public:
        void setup (const params::SynthParamPtrs& p, mod::ModContext& c) { P = &p; ctx = &c; }

        void noteOn (int channel, int midiNote, float velocity) override
        {
            if (velocity <= 0.0f) { noteOff (channel, midiNote, 0.0f, true); return; }

            const int m = mode();
            const bool legato = ! held.empty();
            forget (midiNote);
            held.push_back ({ midiNote, velocity });

            if (m == 0)
            {
                ctx->legatoNow = legato;
                juce::Synthesiser::noteOn (channel, midiNote, velocity);
                return;
            }

            auto* voice = static_cast<SynthVoice*> (getVoice (0));
            if (voice->isVoiceActive())
                voice->retarget (midiNote, velocity, m == 1 || ! legato, legato);
            else
            {
                ctx->legatoNow = legato;
                startVoice (voice, getSound (0).get(), channel, midiNote, velocity);
            }
            sounding = midiNote;
        }

        void noteOff (int channel, int midiNote, float velocity, bool allowTailOff) override
        {
            const bool wasHeld = forget (midiNote);
            const int m = mode();
            if (m == 0)
            {
                juce::Synthesiser::noteOff (channel, midiNote, velocity, allowTailOff);
                return;
            }
            if (! wasHeld)
                return;

            auto* voice = static_cast<SynthVoice*> (getVoice (0));
            if (held.empty())
            {
                if (voice->isVoiceActive()) voice->stopNote (velocity, allowTailOff);
                sounding = -1;
            }
            else if (midiNote == sounding)   // the top note went: fall back to the newest one still held
            {
                const auto back = held.back();
                if (voice->isVoiceActive()) voice->retarget (back.note, back.velocity, m == 1, true);
                sounding = back.note;
            }
        }

        void allNotesOff (int channel, bool allowTailOff) override
        {
            held.clear();
            sounding = -1;
            juce::Synthesiser::allNotesOff (channel, allowTailOff);
        }

        // call once per block: switching mode releases whatever is sounding so nothing is left hanging
        void checkModeChange()
        {
            const int m = mode();
            if (m != lastMode)
            {
                lastMode = m;
                allNotesOff (0, true);
            }
        }

    private:
        struct Held { int note; float velocity; };

        int mode() const { return juce::roundToInt (P->voiceMode->load()); }

        bool forget (int note)
        {
            for (size_t i = 0; i < held.size(); ++i)
                if (held[i].note == note) { held.erase (held.begin() + (std::ptrdiff_t) i); return true; }
            return false;
        }

        const params::SynthParamPtrs* P = nullptr;
        mod::ModContext* ctx = nullptr;
        std::vector<Held> held;
        int sounding = -1, lastMode = 0;
    };
}
