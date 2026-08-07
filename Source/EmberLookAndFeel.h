#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ember::colours
{
    inline const juce::Colour bgDeep   { 0xff0b0e14 };
    inline const juce::Colour bgPanel  { 0xff121824 };
    inline const juce::Colour cyan     { 0xff00f2fe };
    inline const juce::Colour coral    { 0xffff5e36 };
    inline const juce::Colour magenta  { 0xffa100ff };
}

class EmberLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EmberLookAndFeel()
    {
        using namespace ember::colours;
        setColour (juce::ResizableWindow::backgroundColourId, bgDeep);
        setColour (juce::Slider::rotarySliderFillColourId, cyan);
        setColour (juce::Slider::rotarySliderOutlineColourId, bgPanel.brighter (0.15f));
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        setColour (juce::ComboBox::backgroundColourId, bgPanel);
        setColour (juce::ComboBox::outlineColourId, cyan.withAlpha (0.35f));
        setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.9f));
        setColour (juce::ComboBox::arrowColourId, cyan);
        setColour (juce::PopupMenu::backgroundColourId, bgPanel);
        setColour (juce::PopupMenu::textColourId, juce::Colours::white.withAlpha (0.9f));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.25f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (8.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto arcR = radius - 3.0f;
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

        g.setColour (ember::colours::bgPanel);
        g.fillEllipse (bounds.reduced (8.0f));

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto tipR = radius - 14.0f;
        g.drawLine (centre.x + std::sin (angle) * (tipR * 0.35f), centre.y - std::cos (angle) * (tipR * 0.35f),
                    centre.x + std::sin (angle) * tipR,           centre.y - std::cos (angle) * tipR, 2.0f);
    }
};
