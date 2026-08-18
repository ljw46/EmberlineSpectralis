#pragma once

#include "Curve.h"
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

namespace ember::mod
{
    inline constexpr int numLfos = 3;
    inline constexpr int lfoTableSize = 512;
    inline constexpr int maxLfoNodes = 32;

    // x in [0,1] (first = 0, last = 1), y in [-1,1], curve in [-1,1] shapes the segment leaving this node.
    struct LfoNode { float x = 0.0f, y = 0.0f, curve = 0.0f; };

    // Editable LFO paths. Nodes live on the message thread; each edit re-renders a lookup table of
    // atomics that the audio thread reads lock-free (a table can tear for a block mid-edit, which is
    // harmless: every value stays a valid sample in [-1,1]).
    class LfoShapes final : public juce::ChangeBroadcaster
    {
    public:
        LfoShapes()
        {
            setNodes (0, presetNodes (0));
            setNodes (1, presetNodes (1));
            setNodes (2, presetNodes (5));
        }

        // ---- message thread
        std::vector<LfoNode> getNodes (int lfo) const { return nodes[(size_t) lfo]; }

        void setNodes (int lfo, std::vector<LfoNode> n)
        {
            sanitise (n);
            nodes[(size_t) lfo] = n;

            auto& table = tables[(size_t) lfo];
            for (int k = 0; k < lfoTableSize; ++k)
                table[(size_t) k].store (evaluate (n, (float) k / (float) lfoTableSize), std::memory_order_relaxed);

            sendChangeMessage();
        }

        static float evaluate (const std::vector<LfoNode>& n, float x) noexcept
        {
            size_t i = 0;
            while (i + 2 < n.size() && x >= n[i + 1].x)
                ++i;

            const auto& a = n[i];
            const auto& b = n[i + 1];
            const float span = b.x - a.x;
            const float t = span > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (x - a.x) / span) : 0.0f;
            return a.y + (b.y - a.y) * curveShape (t, a.curve);
        }

        static std::vector<LfoNode> presetNodes (int preset)
        {
            std::vector<LfoNode> n;
            switch (preset)
            {
                case 0:   // sine (sampled)
                    for (int k = 0; k <= 16; ++k)
                        n.push_back ({ (float) k / 16.0f, std::sin (juce::MathConstants<float>::twoPi * (float) k / 16.0f), 0.0f });
                    break;
                case 1:   n = { { 0.0f, 0.0f, 0.0f }, { 0.25f, 1.0f, 0.0f }, { 0.75f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }; break;
                case 2:   n = { { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
                case 3:   n = { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
                case 4:   n = { { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 0.501f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
                default:  // random steps
                {
                    juce::Random rng (1234);
                    for (int k = 0; k < 8; ++k)
                    {
                        const float y = rng.nextFloat() * 2.0f - 1.0f;
                        n.push_back ({ (float) k / 8.0f, y, 0.0f });
                        n.push_back ({ k == 7 ? 1.0f : (float) (k + 1) / 8.0f - 0.003f, y, 0.0f });
                    }
                    break;
                }
            }
            return n;
        }

        juce::String toString (int lfo) const
        {
            juce::StringArray parts;
            for (auto& n : nodes[(size_t) lfo])
                parts.add (juce::String (n.x, 5) + "," + juce::String (n.y, 5) + "," + juce::String (n.curve, 4));
            return parts.joinIntoString (";");
        }

        void fromString (int lfo, const juce::String& s)
        {
            std::vector<LfoNode> n;
            for (auto& token : juce::StringArray::fromTokens (s, ";", ""))
            {
                auto f = juce::StringArray::fromTokens (token, ",", "");
                if (f.size() == 3)
                    n.push_back ({ f[0].getFloatValue(), f[1].getFloatValue(), f[2].getFloatValue() });
            }
            setNodes (lfo, n.size() >= 2 ? n : presetNodes (1));
        }

        // ---- audio thread
        float read (int lfo, double phase) const noexcept
        {
            const double p = phase - std::floor (phase);
            const double pos = p * lfoTableSize;
            const int i = (int) pos & (lfoTableSize - 1);
            const float f = (float) (pos - std::floor (pos));
            const auto& t = tables[(size_t) lfo];
            const float a = t[(size_t) i].load (std::memory_order_relaxed);
            const float b = t[(size_t) ((i + 1) & (lfoTableSize - 1))].load (std::memory_order_relaxed);
            return a + (b - a) * f;
        }

    private:
        static void sanitise (std::vector<LfoNode>& n)
        {
            if (n.size() < 2)
                n = presetNodes (1);

            std::stable_sort (n.begin(), n.end(), [] (const LfoNode& a, const LfoNode& b) { return a.x < b.x; });
            if ((int) n.size() > maxLfoNodes)
                n.resize ((size_t) maxLfoNodes);

            for (auto& v : n)
            {
                v.x = juce::jlimit (0.0f, 1.0f, v.x);
                v.y = juce::jlimit (-1.0f, 1.0f, v.y);
                v.curve = juce::jlimit (-1.0f, 1.0f, v.curve);
            }

            n.front().x = 0.0f;
            n.back().x = 1.0f;

            constexpr float eps = 0.001f;
            for (size_t i = 1; i + 1 < n.size();)
            {
                n[i].x = std::max (n[i].x, n[i - 1].x + eps);
                if (n[i].x > 1.0f - eps) n.erase (n.begin() + (std::ptrdiff_t) i);
                else ++i;
            }
        }

        std::array<std::vector<LfoNode>, numLfos> nodes;
        std::array<std::array<std::atomic<float>, lfoTableSize>, numLfos> tables {};
    };
}
