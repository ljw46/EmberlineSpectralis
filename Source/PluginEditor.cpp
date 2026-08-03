#include "PluginEditor.h"

EmberlineEditor::EmberlineEditor (EmberlineProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    initKnob (morph,  ember::params::oscMorph,     "MORPH",  ember::colours::magenta);
    initKnob (cutoff, ember::params::filterCutoff, "CUTOFF", ember::colours::cyan);
    initKnob (volume, ember::params::masterVolume, "VOLUME", ember::colours::coral);

    setSize (520, 300);
}

EmberlineEditor::~EmberlineEditor() { setLookAndFeel (nullptr); }

void EmberlineEditor::initKnob (Knob& k, const juce::String& id, const juce::String& name, juce::Colour accent)
{
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setColour (juce::Slider::thumbColourId, accent);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, k.slider);
}

void EmberlineEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (ember::colours::bgPanel, 0.0f, 0.0f,
                                             ember::colours::bgDeep, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (ember::colours::cyan);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("EMBERLINE SPECTRALIS", getLocalBounds().removeFromTop (50).reduced (20, 0),
                juce::Justification::centredLeft);
}

void EmberlineEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (60).reduced (20);
    const auto w = area.getWidth() / 3;

    for (auto* k : { &morph, &cutoff, &volume })
    {
        auto cell = area.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (20));
        k->slider.setBounds (cell);
    }
}
