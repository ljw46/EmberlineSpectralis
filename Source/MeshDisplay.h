#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include "WavetableOscillator.h"
#include <array>

// Isometric wireframe of the whole wavetable (phase across, frame in depth, waveform as height).
// Each active oscillator's current - modulated - frame is traced across the mesh in its own colour,
// so frame morphing is visible in real time.
class WavetableMesh final : public juce::Component, private juce::Timer
{
public:
    explicit WavetableMesh (EmberlineProcessor& p) : proc (p)
    {
        const auto& bank = proc.getWavetableBank();
        for (int f = 0; f < frames; ++f)
            for (int i = 0; i < points; ++i)
                stack[(size_t) f][(size_t) i] = bank.sample (0, (float) f, (float) i / (float) points);

        for (int o = 0; o < ember::params::numOscs; ++o)
        {
            on[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "on"));
            level[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "level"));
            warpMode[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "warp_mode"));
            warpAmt[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "warp_amt"));
        }

        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff080b10));
        g.fillRoundedRectangle (area, 6.0f);

        const auto plot = area.reduced (14.0f, 10.0f);
        const float amp = plot.getHeight() * 0.13f;

        // projection: phase runs right and slightly down, depth runs right and up
        const float uW = plot.getWidth() * 0.66f, uDrop = plot.getHeight() * 0.22f;
        const float dW = plot.getWidth() * 0.34f, dRise = plot.getHeight() * 0.50f;
        const float ox = plot.getX(), oy = plot.getBottom() - uDrop - amp * 0.4f;
        auto project = [&] (float u, float z, float h) -> juce::Point<float>
        {
            return { ox + u * uW + z * dW, oy + u * uDrop - z * dRise - h * amp };
        };

        juce::ColourGradient neon (ember::colours::cyan, plot.getX(), 0.0f, ember::colours::purple, plot.getRight(), 0.0f, false);
        neon.addColour (0.5, ember::colours::cyan.interpolatedWith (ember::colours::magenta, 0.5f));

        // base grid on the floor
        g.setColour (ember::colours::cyan.withAlpha (0.08f));
        for (int i = 0; i <= 8; ++i)
        {
            const float t = (float) i / 8.0f;
            g.drawLine ({ project (t, 0.0f, -1.2f), project (t, 1.0f, -1.2f) }, 1.0f);
            g.drawLine ({ project (0.0f, t, -1.2f), project (1.0f, t, -1.2f) }, 1.0f);
        }

        std::vector<std::array<juce::Point<float>, (size_t) points + 1>> rowPts ((size_t) rows);
        for (int r = rows - 1; r >= 0; --r)   // back to front
        {
            const float z = (float) r / (float) (rows - 1);
            const float fp = z * (float) (frames - 1);
            const int f0 = juce::jmin ((int) fp, frames - 2);
            const float fr = fp - (float) f0;

            auto& pts = rowPts[(size_t) r];
            for (int i = 0; i <= points; ++i)
            {
                const size_t k = (size_t) (i % points);
                const float h = stack[(size_t) f0][k] * (1.0f - fr) + stack[(size_t) f0 + 1][k] * fr;
                pts[(size_t) i] = project ((float) i / (float) points, z, h);
            }

            juce::Path line, fill;
            line.startNewSubPath (pts[0]);
            fill.startNewSubPath (pts[0]);
            for (size_t i = 1; i < pts.size(); ++i) { line.lineTo (pts[i]); fill.lineTo (pts[i]); }
            fill.lineTo (pts.back().x, plot.getBottom() + 4.0f);
            fill.lineTo (pts.front().x, plot.getBottom() + 4.0f);
            fill.closeSubPath();

            g.setColour (juce::Colour (0xff080b10));
            g.fillPath (fill);

            // cross-lines to the row behind (visible strip between the two rows)
            if (r < rows - 1)
            {
                const auto& behind = rowPts[(size_t) r + 1];
                g.setGradientFill (neon);
                g.setOpacity (0.28f);
                for (int i = 0; i <= points; i += 4)
                    g.drawLine ({ pts[(size_t) i], behind[(size_t) i] }, 1.0f);
            }

            neon.multiplyOpacity (1.0f);
            g.setGradientFill (neon);
            g.setOpacity (0.35f + 0.55f * (1.0f - z));
            g.strokePath (line, juce::PathStrokeType (1.0f));
        }

        // live frame trace per active oscillator
        const juce::Colour oscColours[] { ember::colours::cyan, ember::colours::coral, ember::colours::purple };
        for (int o = 0; o < ember::params::numOscs; ++o)
        {
            if (on[(size_t) o]->load() < 0.5f || level[(size_t) o]->load() < 0.001f)
                continue;

            const auto d = (size_t) ember::mod::oscDest (o, 0);
            const float z = juce::jlimit (0.0f, 1.0f, proc.modPtrs.destBase[d]->load() + proc.modCtx.liveOffset[d].load (std::memory_order_relaxed));
            const float fp = z * (float) (frames - 1);
            const int f0 = juce::jmin ((int) fp, frames - 2);
            const float fr = fp - (float) f0;

            // the trace is one cycle of what the oscillator really plays, i.e. with its warp applied
            const auto wd = (size_t) ember::mod::oscDest (o, 1);
            const float amount = juce::jlimit (0.0f, 1.0f, warpAmt[(size_t) o]->load() + proc.modCtx.liveOffset[wd].load (std::memory_order_relaxed));
            const auto mode = (ember::WarpMode) juce::roundToInt (warpMode[(size_t) o]->load());

            juce::Path trace;
            for (int i = 0; i <= points; ++i)
            {
                float window = 1.0f;
                const float q = ember::WavetableOscillator::warpPhase (mode, (float) i / (float) points, amount, window);
                const float pos = q * (float) points;
                const int k0 = (int) pos % points, k1 = (k0 + 1) % points;
                const float kf = pos - std::floor (pos);
                auto at = [&] (size_t f) { return stack[f][(size_t) k0] * (1.0f - kf) + stack[f][(size_t) k1] * kf; };
                const float h = (at ((size_t) f0) * (1.0f - fr) + at ((size_t) f0 + 1) * fr) * window;
                const auto pt = project ((float) i / (float) points, z, h);
                if (i == 0) trace.startNewSubPath (pt); else trace.lineTo (pt);
            }
            g.setColour (oscColours[o].withAlpha (0.28f));
            g.strokePath (trace, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (oscColours[o].brighter (0.4f));
            g.strokePath (trace, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

private:
    static constexpr int frames = ember::WavetableBank::numFrames;
    static constexpr int points = 64;
    static constexpr int rows = 26;

    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
    std::array<std::atomic<float>*, ember::params::numOscs> on {}, level {}, warpMode {}, warpAmt {};
    std::array<std::array<float, (size_t) points>, (size_t) frames> stack {};
};
