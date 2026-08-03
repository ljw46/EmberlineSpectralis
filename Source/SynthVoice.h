#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace ember
{
    struct SynthSound final : juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    // Placeholder voice: sine -> band-limited saw morph. Replaced by the wavetable engine later.
    class SynthVoice final : public juce::SynthesiserVoice
    {
    public:
        explicit SynthVoice (const std::atomic<float>& morphParam) : morph (morphParam)
        {
            adsr.setParameters ({ 0.005f, 0.2f, 0.7f, 0.25f });
        }

        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

        void setCurrentPlaybackSampleRate (double sr) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (sr);
            adsr.setSampleRate (sr);
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            phase = 0.0;
            level = velocity;
            phaseInc = juce::MidiMessage::getMidiNoteInHertz (midiNote) / getSampleRate();
            adsr.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
                adsr.noteOff();
            else
            {
                adsr.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        using juce::SynthesiserVoice::renderNextBlock;

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive())
                return;

            const auto m = morph.load();

            for (int i = 0; i < num; ++i)
            {
                const auto sine = std::sin (juce::MathConstants<double>::twoPi * phase);
                auto saw = 2.0 * phase - 1.0;
                saw -= polyBlep (phase, phaseInc);

                const auto s = (float) (sine + (saw - sine) * m) * level * adsr.getNextSample() * 0.5f;

                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.addSample (ch, start + i, s);

                phase += phaseInc;
                if (phase >= 1.0)
                    phase -= 1.0;
            }

            if (! adsr.isActive())
                clearCurrentNote();
        }

    private:
        static double polyBlep (double t, double dt)
        {
            if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }

        const std::atomic<float>& morph;
        juce::ADSR adsr;
        double phase = 0.0, phaseInc = 0.0;
        float level = 0.0f;
    };
}
