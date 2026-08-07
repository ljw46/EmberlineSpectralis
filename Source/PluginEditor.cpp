#include "PluginEditor.h"

EmberlineEditor::EmberlineEditor (EmberlineProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    const juce::Colour accents[] { ember::colours::cyan, ember::colours::coral, ember::colours::magenta };
    const char* ids[]   { "frame", "warp_amt", "level", "unison", "detune", "spread", "semi", "fine" };
    const char* names[] { "FRAME", "WARP", "LEVEL", "UNISON", "DETUNE", "SPREAD", "SEMI", "FINE" };

    for (int o = 0; o < ember::params::numOscs; ++o)
    {
        auto& panel = panels[(size_t) o];
        panel.accent = accents[o];

        panel.title.setText ("OSC " + juce::String (o + 1), juce::dontSendNotification);
        panel.title.setColour (juce::Label::textColourId, panel.accent);
        panel.title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        addAndMakeVisible (panel.title);

        panel.warpMode.addItemList (ember::params::warpModeNames, 1);
        addAndMakeVisible (panel.warpMode);
        panel.warpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.apvts, ember::params::osc (o, "warp_mode"), panel.warpMode);

        for (size_t k = 0; k < panel.knobs.size(); ++k)
            panel.knobs[k] = &addKnob (ember::params::osc (o, ids[k]), names[k], panel.accent);
    }

    cutoff = &addKnob (ember::params::filterCutoff, "CUTOFF", ember::colours::cyan);
    volume = &addKnob (ember::params::masterVolume, "VOLUME", ember::colours::coral);

    setSize (960, 620);
}

EmberlineEditor::~EmberlineEditor() { setLookAndFeel (nullptr); }

EmberlineEditor::Knob& EmberlineEditor::addKnob (const juce::String& id, const juce::String& name, juce::Colour accent)
{
    auto& k = knobs.emplace_back();

    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<Attachment> (processor.apvts, id, k.slider);
    return k;
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

    for (auto& panel : panels)
    {
        auto b = panel.bounds.toFloat();
        g.setColour (ember::colours::bgDeep.withAlpha (0.55f));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (panel.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    g.setColour (ember::colours::bgDeep.withAlpha (0.55f));
    g.fillRoundedRectangle (masterBounds.toFloat(), 8.0f);
    g.setColour (ember::colours::cyan.withAlpha (0.2f));
    g.drawRoundedRectangle (masterBounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void EmberlineEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (55).reduced (16, 8);
    auto master = area.removeFromBottom (130);
    masterBounds = master;
    area.removeFromBottom (8);

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> cell)
    {
        k.label.setBounds (cell.removeFromTop (16));
        k.slider.setBounds (cell);
    };

    const int panelW = area.getWidth() / ember::params::numOscs;
    for (auto& panel : panels)
    {
        panel.bounds = area.removeFromLeft (panelW).reduced (4, 0);
        auto inner = panel.bounds.reduced (10, 8);

        auto head = inner.removeFromTop (26);
        panel.title.setBounds (head.removeFromLeft (60));
        panel.warpMode.setBounds (head);
        inner.removeFromTop (6);

        const int rowH = inner.getHeight() / 3;
        for (int row = 0; row < 3; ++row)
        {
            auto r = inner.removeFromTop (rowH);
            const int cellW = r.getWidth() / 3;
            for (int col = 0; col < 3; ++col)
            {
                const size_t idx = (size_t) (row * 3 + col);
                auto cell = r.removeFromLeft (cellW);
                if (idx < panel.knobs.size())
                    placeKnob (*panel.knobs[idx], cell);
            }
        }
    }

    auto m = masterBounds.reduced (10, 8);
    placeKnob (*cutoff, m.removeFromLeft (m.getWidth() / 2));
    placeKnob (*volume, m);
}
