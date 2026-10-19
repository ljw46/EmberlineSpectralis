#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include "VoiceFilter.h"
#include <array>
#include <complex>

// Live frequency-response curve of both filters. The cutoff/resonance shown are the *modulated* values of
// the newest voice, so an envelope or LFO opening and closing the filter is visible as it happens.
class FilterDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit FilterDisplay (EmberlineProcessor& p) : proc (p)
    {
        for (int f = 0; f < 2; ++f)
        {
            type[(size_t) f] = proc.apvts.getRawParameterValue (ember::params::filt (f, "type"));
            slope[(size_t) f] = proc.apvts.getRawParameterValue (ember::params::filt (f, "slope"));
        }
        routing = proc.apvts.getRawParameterValue ("filt_routing");
        startTimerHz (30);
    }

    // magnitude response (linear gain) of a filter at `hz`, matching the maths in VoiceFilter
    static std::complex<float> response (ember::FilterType t, bool steep, float fc, float res, float hz)
    {
        using C = std::complex<float>;
        const C j (0.0f, 1.0f);
        const float w = hz / juce::jmax (1.0f, fc);

        auto svf = [&] (float k, int mode)
        {
            const C den = C (1.0f - w * w, k * w);
            switch (mode)
            {
                case 0:  return C (1.0f) / den;
                case 1:  return C (0.0f, k * w) / den;
                case 2:  return C (-w * w) / den;
                default: return C (1.0f - w * w) / den;
            }
        };

        if (t == ember::FilterType::off)
            return 1.0f;

        if (t == ember::FilterType::ladder)
        {
            const C onePole = C (1.0f) / (C (1.0f) + j * w);
            const C g = onePole * onePole * onePole * onePole;
            return g / (C (1.0f) + res * 3.7f * g);
        }

        if (t == ember::FilterType::formant)
        {
            static constexpr float f1[] { 800.0f, 400.0f, 270.0f, 450.0f, 325.0f }, f2[] { 1150.0f, 2000.0f, 2140.0f, 800.0f, 700.0f };
            const float pos = juce::jlimit (0.0f, 1.0f, std::log (fc / 20.0f) / std::log (1000.0f)) * 4.0f;
            const int i = juce::jmin ((int) pos, 3);
            const float fr = pos - (float) i, k = 0.5f - 0.45f * res;
            C sum;
            for (auto* centres : { f1, f2 })
            {
                const float c = centres[i] + (centres[i + 1] - centres[i]) * fr, ww = hz / c;
                sum += C (0.0f, k * ww) / C (1.0f - ww * ww, k * ww);
            }
            return sum * 1.2f;
        }

        const int mode = t == ember::FilterType::lowpass ? 0 : t == ember::FilterType::bandpass ? 1 : t == ember::FilterType::highpass ? 2 : 3;
        auto h = svf (2.0f - 1.96f * res, mode);
        if (steep)
            h *= svf (1.2f, mode);
        return h;
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff080b10));
        g.fillRoundedRectangle (area, 6.0f);

        const auto plot = area.reduced (34.0f, 22.0f);
        constexpr float fLo = 20.0f, fHi = 20000.0f, dbLo = -42.0f, dbHi = 18.0f;
        auto xOf = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / fLo) / std::log (fHi / fLo); };
        auto yOf = [&] (float db) { return plot.getBottom() - plot.getHeight() * (juce::jlimit (dbLo, dbHi, db) - dbLo) / (dbHi - dbLo); };

        // grid
        g.setFont (juce::FontOptions (10.0f));
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const bool major = juce::approximatelyEqual (hz, 100.0f) || juce::approximatelyEqual (hz, 1000.0f) || juce::approximatelyEqual (hz, 10000.0f);
            g.setColour (ember::colours::cyan.withAlpha (major ? 0.14f : 0.06f));
            g.drawVerticalLine ((int) xOf (hz), plot.getY(), plot.getBottom());
            if (major)
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                            juce::Rectangle<float> (xOf (hz) - 20.0f, plot.getBottom() + 3.0f, 40.0f, 14.0f), juce::Justification::centred);
            }
        }
        for (float db : { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f })
        {
            g.setColour (ember::colours::cyan.withAlpha (juce::approximatelyEqual (db, 0.0f) ? 0.2f : 0.07f));
            g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (2.0f, yOf (db) - 7.0f, plot.getX() - 6.0f, 14.0f), juce::Justification::centredRight);
        }

        const std::array<juce::Colour, 2> colours { ember::colours::cyan, ember::colours::purple };
        const bool parallel = routing->load() > 0.5f;
        std::array<float, 2> cutoff {}, res {};
        std::array<bool, 2> active {};

        for (int f = 0; f < 2; ++f)
        {
            const auto fi = (size_t) f;
            active[fi] = juce::roundToInt (type[fi]->load()) != 0;
            const auto cd = (size_t) ember::mod::filterDest (f, 0), rd = (size_t) ember::mod::filterDest (f, 1);
            cutoff[fi] = modulated (cd);
            res[fi] = juce::jlimit (0.0f, 1.0f, modulated (rd));
        }

        const int steps = juce::jmax (8, (int) plot.getWidth() / 2);
        juce::Path combined;
        std::array<juce::Path, 2> curves;
        for (int i = 0; i <= steps; ++i)
        {
            const float hz = fLo * std::pow (fHi / fLo, (float) i / (float) steps);
            std::array<std::complex<float>, 2> h { std::complex<float> (1.0f), std::complex<float> (1.0f) };
            for (size_t f = 0; f < 2; ++f)
                if (active[f])
                    h[f] = response ((ember::FilterType) juce::roundToInt (type[f]->load()), slope[f]->load() > 0.5f, cutoff[f], res[f], hz);

            const float x = plot.getX() + plot.getWidth() * (float) i / (float) steps;
            for (size_t f = 0; f < 2; ++f)
            {
                if (! active[f])
                    continue;
                const float y = yOf (20.0f * std::log10 (juce::jmax (1.0e-5f, std::abs (h[f]))));
                if (curves[f].isEmpty()) curves[f].startNewSubPath (x, y); else curves[f].lineTo (x, y);
            }

            const auto total = parallel ? (active[0] && active[1] ? (h[0] + h[1]) * 0.7071f : active[0] ? h[0] : h[1]) : h[0] * h[1];
            const float y = yOf (20.0f * std::log10 (juce::jmax (1.0e-5f, std::abs (total))));
            if (i == 0) combined.startNewSubPath (x, y); else combined.lineTo (x, y);
        }

        // combined response: filled
        if (active[0] || active[1])
        {
            auto fill = combined;
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            juce::ColourGradient grad (ember::colours::cyan.withAlpha (0.28f), 0.0f, plot.getY(), ember::colours::purple.withAlpha (0.04f), 0.0f, plot.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (fill);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.strokePath (combined, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        for (size_t f = 0; f < 2; ++f)
        {
            if (! active[f])
                continue;

            if (active[0] && active[1])   // per-filter curves only matter when there are two
            {
                g.setColour (colours[f].withAlpha (0.55f));
                g.strokePath (curves[f], juce::PathStrokeType (1.2f));
            }

            const float x = xOf (cutoff[f]);
            g.setColour (colours[f].withAlpha (0.8f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.fillEllipse (x - 4.0f, yOf (0.0f) - 4.0f, 8.0f, 8.0f);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            const auto text = "F" + juce::String ((int) f + 1) + "  " + (cutoff[f] >= 1000.0f ? juce::String (cutoff[f] / 1000.0f, 2) + " kHz" : juce::String ((int) cutoff[f]) + " Hz");
            g.drawText (text, juce::Rectangle<float> (juce::jlimit (plot.getX(), plot.getRight() - 90.0f, x + 6.0f), plot.getY() + 2.0f + (float) f * 14.0f, 90.0f, 14.0f),
                        juce::Justification::centredLeft);
        }

        if (! active[0] && ! active[1])
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("FILTERS OFF", plot.toNearestInt(), juce::Justification::centred);
        }

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.08f));
        g.drawText ("FILTER RESPONSE  " + juce::String (parallel ? "PARALLEL" : "SERIAL"), getLocalBounds().reduced (12, 4).removeFromTop (16), juce::Justification::centredLeft);
    }

private:
    float modulated (size_t dest) const
    {
        const float base = proc.modPtrs.destBase[dest]->load();
        const float live = proc.modCtx.liveOffset[dest].load (std::memory_order_relaxed);
        const auto& r = proc.modPtrs.destRange[dest];
        return r.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, r.convertTo0to1 (base) + live));
    }

    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
    std::array<std::atomic<float>*, 2> type {}, slope {};
    std::atomic<float>* routing = nullptr;
};
