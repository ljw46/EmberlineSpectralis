#include "PluginEditor.h"

using namespace ember;

EmberlineEditor::EmberlineEditor (EmberlineProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    const juce::Colour accents[] { colours::cyan, colours::coral, colours::magenta };
    const char* ids[]   { "frame", "warp_amt", "level", "unison", "detune", "spread", "semi", "fine" };
    const char* names[] { "FRAME", "WARP", "LEVEL", "UNISON", "DETUNE", "SPREAD", "SEMI", "FINE" };

    for (int o = 0; o < params::numOscs; ++o)
    {
        auto& panel = addPanel ("OSC " + juce::String (o + 1), accents[o], 3);
        addOnSwitch (panel, params::osc (o, "on"));
        addCombo (panel, params::osc (o, "warp_mode"), params::warpModeNames);
        for (size_t k = 0; k < std::size (ids); ++k)
            addKnob (panel, params::osc (o, ids[k]), names[k]);
        oscRow.push_back (&panel);
    }

    auto& sub = addPanel ("SUB / NOISE", colours::coral);
    addCombo (sub, "sub_wave", params::subWaveNames);
    addCombo (sub, "sub_oct", params::subOctNames);
    addCombo (sub, "noise_type", params::noiseTypeNames);
    addKnob (sub, "sub_level", "SUB");
    addKnob (sub, "noise_level", "NOISE");
    lowerRow.push_back (&sub);

    for (int f = 0; f < 2; ++f)
    {
        auto& panel = addPanel ("FILTER " + juce::String (f + 1), f == 0 ? colours::cyan : colours::magenta);
        addCombo (panel, params::filt (f, "type"), params::filterTypeNames);
        addKnob (panel, params::filt (f, "cutoff"), "CUTOFF");
        addKnob (panel, params::filt (f, "res"), "RES");
        addKnob (panel, params::filt (f, "drive"), "DRIVE");
        lowerRow.push_back (&panel);
    }

    auto& master = addPanel ("MASTER", colours::cyan);
    addCombo (master, "filt_routing", params::routingNames);
    addKnob (master, params::masterVolume, "VOLUME");
    lowerRow.push_back (&master);

    setSize (960, 640);
}

EmberlineEditor::~EmberlineEditor() { setLookAndFeel (nullptr); }

EmberlineEditor::Panel& EmberlineEditor::addPanel (const juce::String& title, juce::Colour accent, int columns)
{
    auto& panel = panels.emplace_back();
    panel.accent = accent;
    panel.columns = columns;
    panel.title.setText (title, juce::dontSendNotification);
    panel.title.setColour (juce::Label::textColourId, accent);
    panel.title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (panel.title);
    return panel;
}

void EmberlineEditor::addKnob (Panel& panel, const juce::String& id, const juce::String& name)
{
    auto& k = knobStore.emplace_back();

    k.slider.setColour (juce::Slider::rotarySliderFillColourId, panel.accent);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, id, k.slider);
    panel.knobs.push_back (&k);
}

void EmberlineEditor::addOnSwitch (Panel& panel, const juce::String& id)
{
    panel.onButton.setButtonText ("ON");
    panel.onButton.setColour (juce::ToggleButton::tickColourId, panel.accent);
    panel.onButton.setColour (juce::ToggleButton::tickDisabledColourId, panel.accent.withAlpha (0.3f));
    panel.onButton.setColour (juce::ToggleButton::textColourId, panel.accent);
    addAndMakeVisible (panel.onButton);
    panel.onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, panel.onButton);
    hasOnSwitch.push_back (&panel);
}

void EmberlineEditor::addCombo (Panel& panel, const juce::String& id, const juce::StringArray& items)
{
    auto& c = comboStore.emplace_back();
    c.box.addItemList (items, 1);
    addAndMakeVisible (c.box);
    c.attachment = std::make_unique<ComboAttachment> (processor.apvts, id, c.box);
    panel.combos.push_back (&c);
}

void EmberlineEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bgPanel, 0.0f, 0.0f, colours::bgDeep, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (colours::cyan);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("EMBERLINE SPECTRALIS", getLocalBounds().removeFromTop (50).reduced (20, 0), juce::Justification::centredLeft);

    for (auto& panel : panels)
    {
        const auto b = panel.bounds.toFloat();
        g.setColour (colours::bgDeep.withAlpha (0.55f));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (panel.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }
}

void EmberlineEditor::layoutPanel (Panel& panel)
{
    auto inner = panel.bounds.reduced (10, 8);
    auto head = inner.removeFromTop (20);
    if (std::find (hasOnSwitch.begin(), hasOnSwitch.end(), &panel) != hasOnSwitch.end())
        panel.onButton.setBounds (head.removeFromRight (56));
    panel.title.setBounds (head);

    if (! panel.combos.empty())
    {
        auto row = inner.removeFromTop (24);
        const int w = row.getWidth() / (int) panel.combos.size();
        for (auto* c : panel.combos)
            c->box.setBounds (row.removeFromLeft (w).reduced (2, 0));
        inner.removeFromTop (4);
    }

    const int n = (int) panel.knobs.size();
    const int cols = panel.columns > 0 ? panel.columns : juce::jmax (1, n);
    const int rows = (n + cols - 1) / cols;
    const int rowH = inner.getHeight() / juce::jmax (1, rows);

    for (int r = 0; r < rows; ++r)
    {
        auto row = inner.removeFromTop (rowH);
        const int cellW = row.getWidth() / cols;
        for (int c = 0; c < cols && r * cols + c < n; ++c)
        {
            auto cell = row.removeFromLeft (cellW);
            auto& k = *panel.knobs[(size_t) (r * cols + c)];
            k.label.setBounds (cell.removeFromTop (16));
            k.slider.setBounds (cell);
        }
    }
}

void EmberlineEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (55).reduced (16, 8);
    auto top = area.removeFromTop (area.getHeight() * 62 / 100);
    area.removeFromTop (8);

    const int w = top.getWidth() / (int) oscRow.size();
    for (auto* p : oscRow)
    {
        p->bounds = top.removeFromLeft (w).reduced (4, 0);
        layoutPanel (*p);
    }

    const float weights[] { 1.15f, 1.35f, 1.35f, 0.9f };
    float total = 0.0f;
    for (auto v : weights) total += v;

    const int fullW = area.getWidth();
    for (size_t i = 0; i < lowerRow.size(); ++i)
    {
        lowerRow[i]->bounds = area.removeFromLeft ((int) ((float) fullW * weights[i] / total)).reduced (4, 0);
        layoutPanel (*lowerRow[i]);
    }
}
