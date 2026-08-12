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
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        std::unique_ptr<ComboAttachment> attachment;
    };

    struct Panel
    {
        juce::Colour accent;
        int columns = 0;                 // knob columns; 0 = one row
        juce::Rectangle<int> bounds;
        juce::Label title;
        juce::ToggleButton onButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        std::vector<Combo*> combos;
        std::vector<Knob*> knobs;
    };

    Panel& addPanel (const juce::String& title, juce::Colour accent, int columns = 0);
    void addKnob (Panel&, const juce::String& paramId, const juce::String& name);
    void addOnSwitch (Panel&, const juce::String& paramId);
    void addCombo (Panel&, const juce::String& paramId, const juce::StringArray& items);
    void layoutPanel (Panel&);

    EmberlineProcessor& processor;
    EmberLookAndFeel lnf;
    std::deque<Knob> knobStore;     // stable addresses
    std::deque<Combo> comboStore;
    std::deque<Panel> panels;
    std::vector<Panel*> oscRow, lowerRow, hasOnSwitch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberlineEditor)
};
