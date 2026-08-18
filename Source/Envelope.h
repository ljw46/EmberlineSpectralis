#pragma once

#include "Curve.h"
#include <algorithm>
#include <cmath>

namespace ember
{
    // ADSR with shaped attack and decay slopes. Advances by an arbitrary number of samples so the
    // amp envelope can run per-sample while modulation envelopes tick once per control chunk.
    class Envelope
    {
    public:
        struct Params
        {
            float attack = 0.005f, decay = 0.2f, sustain = 0.7f, release = 0.25f;
            float attackCurve = 0.0f, decayCurve = 0.4f;
        };

        void setSampleRate (double sr) noexcept { sampleRate = (float) sr; }
        void setParams (const Params& p) noexcept { params = p; }

        void noteOn() noexcept   { stage = Stage::attack; t = 0.0f; startLevel = level; }
        void reset() noexcept    { stage = Stage::idle; level = 0.0f; t = 0.0f; }
        void noteOff() noexcept
        {
            if (stage != Stage::idle && stage != Stage::release)
            {
                stage = Stage::release;
                t = 0.0f;
                releaseLevel = level;
            }
        }

        bool isActive() const noexcept { return stage != Stage::idle; }
        float value() const noexcept   { return level; }

        float process (int numSamples) noexcept
        {
            float remaining = (float) numSamples;

            while (remaining > 0.0f && stage != Stage::idle)
            {
                switch (stage)
                {
                    case Stage::attack:
                    {
                        const float dur = std::max (params.attack * sampleRate, 1.0f);
                        const float need = (1.0f - t) * dur;
                        if (remaining >= need)
                        {
                            remaining -= need;
                            level = 1.0f;
                            t = 0.0f;
                            stage = Stage::decay;
                        }
                        else
                        {
                            t += remaining / dur;
                            remaining = 0.0f;
                            level = startLevel + (1.0f - startLevel) * curveShape (t, params.attackCurve);
                        }
                        break;
                    }

                    case Stage::decay:
                    {
                        const float dur = std::max (params.decay * sampleRate, 1.0f);
                        const float need = (1.0f - t) * dur;
                        if (remaining >= need)
                        {
                            remaining -= need;
                            level = params.sustain;
                            t = 0.0f;
                            stage = Stage::sustain;
                        }
                        else
                        {
                            t += remaining / dur;
                            remaining = 0.0f;
                            level = 1.0f - (1.0f - params.sustain) * curveShape (t, params.decayCurve);
                        }
                        break;
                    }

                    case Stage::sustain:
                        if (params.sustain < 1.0e-4f)   // zero sustain = the note ends after the decay (plucks)
                        {
                            level = 0.0f;
                            stage = Stage::idle;
                        }
                        else
                        {
                            level += (params.sustain - level) * (1.0f - std::pow (0.995f, remaining));
                        }
                        remaining = 0.0f;
                        break;

                    case Stage::release:
                    {
                        const float dur = std::max (params.release * sampleRate, 1.0f);
                        const float need = (1.0f - t) * dur;
                        if (remaining >= need)
                        {
                            level = 0.0f;
                            stage = Stage::idle;
                        }
                        else
                        {
                            t += remaining / dur;
                            level = releaseLevel * (1.0f - curveShape (t, 0.5f));
                        }
                        remaining = 0.0f;
                        break;
                    }

                    case Stage::idle:
                        remaining = 0.0f;
                        break;
                }
            }

            return level;
        }

    private:
        enum class Stage { idle, attack, decay, sustain, release };

        Params params;
        Stage stage = Stage::idle;
        float sampleRate = 44100.0f, level = 0.0f, t = 0.0f, startLevel = 0.0f, releaseLevel = 0.0f;
    };
}
