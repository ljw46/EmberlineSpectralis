#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include "WavetableOscillator.h"
#include <array>

// Isometric wireframe of a wavetable (phase across, frame in depth, waveform as height). The OSC 1/2/3
// buttons choose whose table is shown; every active oscillator using that table traces its current -
// modulated and warped - frame across the mesh in its own colour, so frame morphing is visible live.
class WavetableMesh final : public juce::Component, private juce::Timer
{
public:
    explicit WavetableMesh (EmberlineProcessor& p) : proc (p)
    {
        for (int o = 0; o < ember::params::numOscs; ++o)
        {
            on[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "on"));
            level[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "level"));
            tableParam[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "table"));
            warpMode[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "warp_mode"));
            warpAmt[(size_t) o] = proc.apvts.getRawParameterValue (ember::params::osc (o, "warp_amt"));

            auto& b = oscButtons[(size_t) o];
            b.setButtonText ("OSC " + juce::String (o + 1));
            b.setClickingTogglesState (true);
            b.setRadioGroupId (77);
            b.setColour (juce::TextButton::buttonOnColourId, oscColour (o).withAlpha (0.25f));
            b.setColour (juce::TextButton::textColourOnId, oscColour (o));
            b.onClick = [this, o] { selectedOsc = o; repaint(); };
            addAndMakeVisible (b);
        }
        oscButtons[0].setToggleState (true, juce::dontSendNotification);
        startTimerHz (30);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6).removeFromTop (22);
        for (auto& b : oscButtons)
            b.setBounds (r.removeFromLeft (58).reduced (2, 0));
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff080b10));
        g.fillRoundedRectangle (area, 6.0f);

        auto& bank = proc.getWavetableBank();
        const int table = juce::jlimit (0, bank.numSlots() - 1, juce::roundToInt (tableParam[(size_t) selectedOsc]->load()));
        ensureStack (table);

        const auto plot = area.reduced (14.0f, 10.0f).withTrimmedTop (22.0f);
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

            g.setGradientFill (neon);
            g.setOpacity (0.35f + 0.55f * (1.0f - z));
            g.strokePath (line, juce::PathStrokeType (1.0f));
        }

        // live frame trace for every active oscillator that plays this table
        for (int o = 0; o < ember::params::numOscs; ++o)
        {
            if (on[(size_t) o]->load() < 0.5f || level[(size_t) o]->load() < 0.001f
                || juce::roundToInt (tableParam[(size_t) o]->load()) != table)
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
            g.setColour (oscColour (o).withAlpha (0.28f));
            g.strokePath (trace, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (oscColour (o).brighter (0.4f));
            g.strokePath (trace, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // table name
        const auto names = bank.displayNames();
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (names[table], getLocalBounds().reduced (10, 6).removeFromTop (22), juce::Justification::centredRight);
    }

private:
    static constexpr int frames = ember::WavetableBank::numFrames;
    static constexpr int points = 64;
    static constexpr int rows = 26;

    static juce::Colour oscColour (int o)
    {
        const juce::Colour c[] { ember::colours::cyan, ember::colours::coral, ember::colours::purple };
        return c[o];
    }

    // the surface samples the chosen table; refreshed when the table (or its user file) changes
    void ensureStack (int table)
    {
        auto& bank = proc.getWavetableBank();
        const auto& info = bank.info (table);
        const auto key = juce::String (table) + info.name + juce::String (bank.table (table, 0, 0)[37]) + juce::String (bank.table (table, 0, frames - 1)[101]);
        if (key == stackKey)
            return;

        stackKey = key;
        for (int f = 0; f < frames; ++f)
            for (int i = 0; i < points; ++i)
                stack[(size_t) f][(size_t) i] = bank.table (table, 0, f)[(size_t) i * (ember::WavetableBank::tableSize / points)];
    }

    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
    int selectedOsc = 0;
    juce::String stackKey;
    std::array<juce::TextButton, ember::params::numOscs> oscButtons;
    std::array<std::atomic<float>*, ember::params::numOscs> on {}, level {}, tableParam {}, warpMode {}, warpAmt {};
    std::array<std::array<float, (size_t) points>, (size_t) frames> stack {};
};
