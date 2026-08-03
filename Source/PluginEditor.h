#pragma once

#include "PluginProcessor.h"
#include "EmberLookAndFeel.h"

class EmberlineEditor final : public juce::AudioProcessorEditor
{
public:
    explicit EmberlineEditor (EmberlineProcessor&);
    ~EmberlineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void initKnob (Knob&, const juce::String& paramId, const juce::String& name, juce::Colour accent);

    EmberlineProcessor& processor;
    EmberLookAndFeel lnf;
    Knob morph, cutoff, volume;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineEditor)
};
