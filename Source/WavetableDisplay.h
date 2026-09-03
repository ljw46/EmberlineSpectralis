#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include <array>

// 3D view of the wavetable stack: every frame as a ribbon in perspective over a neon floor grid,
// with a bright ribbon at the oscillator's current (modulated) frame position.
class WavetableDisplay final : public juce::Component, private juce::Timer
{
public:
    WavetableDisplay (EmberlineProcessor& p, int oscIndex, juce::Colour c) : proc (p), osc (oscIndex), accent (c)
    {
        const auto& bank = proc.getWavetableBank();
        for (int f = 0; f < frames; ++f)
            for (int i = 0; i < points; ++i)
                stack[(size_t) f][(size_t) i] = bank.sample (0, (float) f, (float) i / (float) points);

        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (ember::colours::bgDeep);
        g.fillRoundedRectangle (area, 5.0f);

        const auto plot = area.reduced (8.0f, 6.0f);
        const float depthH = plot.getHeight() * 0.5f;
        const float skew = plot.getWidth() * 0.14f;

        // frame position in 0..1 including live modulation
        const float live = proc.modCtx.liveOffset[(size_t) ember::mod::oscDest (osc, 0)].load (std::memory_order_relaxed);
        const float base = proc.modPtrs.destBase[(size_t) ember::mod::oscDest (osc, 0)]->load();
        const float pos = juce::jlimit (0.0f, 1.0f, base + live);

        // geometry for a given depth z (0 = front, 1 = back)
        struct Row { float left, width, baseline, amp; };
        auto row = [&] (float z) -> Row
        {
            const float shrink = 1.0f - 0.2f * z;
            return { plot.getX() + skew * z + plot.getWidth() * (1.0f - shrink - 0.14f) * 0.5f * z,
                     (plot.getWidth() - skew) * shrink,
                     plot.getBottom() - 2.0f - depthH * z,
                     plot.getHeight() * 0.27f * shrink };
        };

        // floor grid
        g.setColour (accent.withAlpha (0.13f));
        for (int i = 0; i <= 4; ++i)
        {
            const auto r = row ((float) i / 4.0f);
            g.drawHorizontalLine ((int) r.baseline, r.left, r.left + r.width);
        }
        const auto front = row (0.0f), back = row (1.0f);
        for (int i = 0; i <= 8; ++i)
        {
            const float t = (float) i / 8.0f;
            g.drawLine (front.left + front.width * t, front.baseline, back.left + back.width * t, back.baseline, 1.0f);
        }

        auto ribbon = [&] (float z, const float* wave)
        {
            const auto r = row (z);
            juce::Path path;
            for (int i = 0; i <= points; ++i)
            {
                const float v = wave[i % points];
                const float x = r.left + r.width * (float) i / (float) points;
                const float y = r.baseline - v * r.amp;
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            return path;
        };

        // a subset of the frames, back to front so nearer ribbons hide the ones behind
        for (int k = ribbons - 1; k >= 0; --k)
        {
            const float z = (float) k / (float) (ribbons - 1);
            std::array<float, (size_t) points> wave {};
            blend (z, wave);
            auto path = ribbon (z, wave.data());

            auto fill = path;
            const auto r = row (z);
            fill.lineTo (r.left + r.width, r.baseline + 3.0f);
            fill.lineTo (r.left, r.baseline + 3.0f);
            fill.closeSubPath();
            g.setColour (ember::colours::bgDeep);
            g.fillPath (fill);

            g.setColour (accent.withAlpha (0.18f + 0.4f * (1.0f - z)));
            g.strokePath (path, juce::PathStrokeType (1.0f));
        }

        // current frame, interpolated
        std::array<float, (size_t) points> cur {};
        blend (pos, cur);

        const auto live2 = ribbon (pos, cur.data());
        g.setColour (accent.withAlpha (0.3f));
        g.strokePath (live2, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (live2, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    static constexpr int frames = ember::WavetableBank::numFrames;
    static constexpr int points = 96;
    static constexpr int ribbons = 9;

    // interpolated waveform at normalised frame position z (0..1)
    void blend (float z, std::array<float, (size_t) points>& out) const
    {
        const float fp = z * (float) (frames - 1);
        const int f0 = juce::jmin ((int) fp, frames - 2);
        const float fr = fp - (float) f0;
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = stack[(size_t) f0][i] * (1.0f - fr) + stack[(size_t) f0 + 1][i] * fr;
    }

    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
    int osc;
    juce::Colour accent;
    std::array<std::array<float, (size_t) points>, (size_t) frames> stack {};
};
