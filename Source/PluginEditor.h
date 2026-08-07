#pragma once

#include "PluginProcessor.h"
#include "EmberLookAndFeel.h"
#include <deque>

class EmberlineEditor final : public juce::AudioProcessorEditor
{
public:
    explicit EmberlineEditor (EmberlineProcessor&);
    ~EmberlineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<Attachment> attachment;
    };

    struct OscPanel
    {
        juce::Colour accent;
        juce::Rectangle<int> bounds;
        juce::Label title;
        juce::ComboBox warpMode;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> warpAttachment;
        std::array<Knob*, 8> knobs {};   // frame, warp, level, unison, detune, spread, semi, fine
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& name, juce::Colour accent);

    EmberlineProcessor& processor;
    EmberLookAndFeel lnf;
    std::deque<Knob> knobs;   // stable addresses
    std::array<OscPanel, ember::params::numOscs> panels;
    Knob *cutoff = nullptr, *volume = nullptr;
    juce::Rectangle<int> masterBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineEditor)
};
