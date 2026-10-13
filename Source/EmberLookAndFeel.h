#pragma once

#include "Modulation.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace ember::colours
{
    inline const juce::Colour bgDeep    { 0xff0b0e14 };
    inline const juce::Colour bgPanel   { 0xff121824 };
    inline const juce::Colour panelFill { 0xff0e131d };
    inline const juce::Colour cyan      { 0xff00f2fe };
    inline const juce::Colour coral     { 0xffff5e36 };
    inline const juce::Colour purple    { 0xffa100ff };
    inline const juce::Colour magenta   { 0xffe040fb };   // chaos + modulation controls
    inline const juce::Colour gold      { 0xffffc933 };
    inline const juce::Colour amber     { 0xffff9f2e };   // performance macros (mockup orange)
    inline const juce::Colour steel     { 0xff6fa8dc };   // master controls
    inline const juce::Colour lime      { 0xff8cff9e };   // performance sources (velocity, key, wheel, aftertouch, random)

    // Colour code for modulation sources: LFO = magenta, envelope = coral, macro = cyan, chaos = gold, performance = green
    inline juce::Colour sourceColour (int src)
    {
        switch (mod::sourceKind (src))
        {
            case mod::SourceKind::lfo:   return magenta;
            case mod::SourceKind::env:   return coral;
            case mod::SourceKind::macro: return cyan;
            case mod::SourceKind::chaos: return gold;
            case mod::SourceKind::perform: return lime;
        }
        return cyan;
    }
}

namespace ember
{
    // Shared rotary geometry so the knob and its modulation arcs line up.
    struct KnobGeometry
    {
        juce::Point<float> centre;
        float radius;   // outer radius of the value arc
    };

    inline KnobGeometry knobGeometry (juce::Rectangle<int> sliderBounds)
    {
        const auto b = sliderBounds.toFloat();
        return { b.getCentre(), juce::jmax (4.0f, juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 6.0f) };
    }
}

class EmberLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EmberLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};
