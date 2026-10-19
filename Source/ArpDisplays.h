#pragma once

#include "Arp.h"
#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"

// 16-step rhythm pattern of the arpeggiator: tall bar = accent, short = normal, dot = rest.
class ArpPatternDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit ArpPatternDisplay (EmberlineProcessor& p) : pattern (p.apvts.getRawParameterValue ("arp_pattern")), on (p.apvts.getRawParameterValue ("arp_on"))
    {
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff080b10));
        g.fillRoundedRectangle (area, 6.0f);

        const char* steps = ember::arp::patternSteps[juce::jlimit (0, (int) std::size (ember::arp::patternSteps) - 1, juce::roundToInt (pattern->load()))];
        const auto plot = area.reduced (12.0f, 8.0f);
        const float cell = plot.getWidth() / 16.0f;
        const auto colour = ember::colours::cyan.withAlpha (on->load() > 0.5f ? 1.0f : 0.45f);

        for (int i = 0; i < 16; ++i)
        {
            const float x = plot.getX() + (float) i * cell;
            if (i % 4 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
            }

            const float h = steps[i] == 'X' ? plot.getHeight() : steps[i] == 'x' ? plot.getHeight() * 0.55f : 0.0f;
            if (h > 0.0f)
            {
                g.setColour (colour.withAlpha (steps[i] == 'X' ? 0.95f : 0.6f));
                g.fillRoundedRectangle (x + 2.0f, plot.getBottom() - h, cell - 4.0f, h, 2.5f);
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillEllipse (x + cell * 0.5f - 2.0f, plot.getBottom() - 5.0f, 4.0f, 4.0f);
            }
        }
    }

private:
    void timerCallback() override { repaint(); }
    std::atomic<float> *pattern, *on;
};

// One stutter cycle: a tick for every note the arp will play, so the speeding-up / slowing-down roll is visible.
class StutterDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit StutterDisplay (EmberlineProcessor& p) : proc (p)
    {
        for (int i = 0; i < 6; ++i)
            ptr[(size_t) i] = proc.apvts.getRawParameterValue (names[i]);
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff080b10));
        g.fillRoundedRectangle (area, 6.0f);

        const int rate = juce::roundToInt (ptr[0]->load()), mode = juce::roundToInt (ptr[1]->load()), len = juce::roundToInt (ptr[3]->load());
        const float depth = ptr[2]->load();
        const bool stepped = ptr[4]->load() > 0.5f, active = ptr[5]->load() > 0.5f;
        const double base = ember::arp::rateBeats[juce::jlimit (0, (int) std::size (ember::arp::rateBeats) - 1, rate)];
        const double cycle = ember::arp::stutterLenBeats[juce::jlimit (0, 3, len)];

        const auto plot = area.reduced (12.0f, 8.0f);
        const auto colour = ember::colours::coral.withAlpha (active ? 1.0f : 0.5f);

        // rate curve
        juce::Path curve;
        const int n = 96;
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n;
            const float f = ember::arp::stutterFactor (mode, depth, stepped, ph);
            const float y = plot.getBottom() - plot.getHeight() * 0.9f * std::log2 (f) / 3.0f - 2.0f;
            if (i == 0) curve.startNewSubPath (plot.getX(), y); else curve.lineTo (plot.getX() + plot.getWidth() * ph, y);
        }
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (1.2f));

        // note ticks
        double t = 0.0;
        int count = 0;
        while (t < cycle && count++ < 400)
        {
            const float ph = (float) (t / cycle);
            const float f = mode == 0 ? 1.0f : ember::arp::stutterFactor (mode, depth, stepped, ph);
            const float x = plot.getX() + plot.getWidth() * ph;
            const float h = plot.getHeight() * (0.25f + 0.55f * (mode == 0 ? 0.0f : ember::arp::stutterShape (mode, ph)));
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (x, plot.getBottom() - h, 1.8f, h, 0.8f);
            t += base / (double) f;
        }

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (mode == 0 ? "STUTTER OFF" : "ONE CYCLE", getLocalBounds().reduced (10, 4).removeFromTop (14), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }

    static constexpr const char* names[] { "arp_rate", "arp_stutter_mode", "arp_stutter_depth", "arp_stutter_length", "arp_stutter_curve", "arp_on" };
    EmberlineProcessor& proc;
    std::array<std::atomic<float>*, 6> ptr {};
};
