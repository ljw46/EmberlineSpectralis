#include "PluginEditor.h"

using namespace ember;

EmberlineEditor::EmberlineEditor (EmberlineProcessor& p)
    : AudioProcessorEditor (&p), processor (p), chaosDisplay (p), matrix (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (soundPage);
    addChildComponent (modPage);
    addChildComponent (fxPage);

    // ---- SOUND page
    const juce::Colour accents[] { colours::cyan, colours::coral, colours::magenta };
    const char* ids[]   { "frame", "warp_amt", "level", "unison", "detune", "spread", "semi", "fine", "pan" };
    const char* names[] { "FRAME", "WARP", "LEVEL", "UNISON", "DETUNE", "SPREAD", "SEMI", "FINE", "PAN" };

    for (int o = 0; o < params::numOscs; ++o)
    {
        auto& panel = addPanel (soundPage, "OSC " + juce::String (o + 1), accents[o], 3);
        panel.addOnSwitch (params::osc (o, "on"));
        panel.addCombo (params::osc (o, "warp_mode"), params::warpModeNames);
        for (size_t k = 0; k < std::size (ids); ++k)
            panel.addKnob (params::osc (o, ids[k]), names[k]);
        oscRow.push_back (&panel);
    }

    auto& sub = addPanel (soundPage, "SUB / NOISE", colours::coral);
    sub.addCombo ("sub_wave", params::subWaveNames);
    sub.addCombo ("sub_oct", params::subOctNames);
    sub.addCombo ("noise_type", params::noiseTypeNames);
    sub.addKnob ("sub_level", "SUB");
    sub.addKnob ("noise_level", "NOISE");
    lowerRow.push_back (&sub);

    for (int f = 0; f < 2; ++f)
    {
        auto& panel = addPanel (soundPage, "FILTER " + juce::String (f + 1), f == 0 ? colours::cyan : colours::magenta);
        panel.addCombo (params::filt (f, "type"), params::filterTypeNames);
        panel.addKnob (params::filt (f, "cutoff"), "CUTOFF");
        panel.addKnob (params::filt (f, "res"), "RES");
        panel.addKnob (params::filt (f, "drive"), "DRIVE");
        lowerRow.push_back (&panel);
    }

    auto& master = addPanel (soundPage, "MASTER", colours::cyan);
    master.addCombo ("filt_routing", params::routingNames);
    master.addKnob (params::masterVolume, "VOLUME");
    lowerRow.push_back (&master);

    // ---- MOD page
    for (int i = 0; i < mod::numLfos; ++i)
    {
        const auto colour = colours::sourceColour (mod::srcLfo + i);
        auto& editor = lfoEditors.emplace_back (processor, i, colour);
        auto& panel = addPanel (modPage, "LFO " + juce::String (i + 1), colour);
        panel.setWidget (editor, 0, true, 0.64f);
        panel.addCombo ("lfo" + juce::String (i + 1) + "_mode", mod::lfoModeNames);
        panel.addKnob ("lfo" + juce::String (i + 1) + "_rate", "RATE");
        lfoRow.push_back (&panel);
    }

    const char* envIds[]   { "attack", "decay", "sustain", "release", "acurve", "dcurve" };
    const char* envNames[] { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "A CURVE", "D CURVE" };
    for (int i = 0; i < mod::numEnvs; ++i)
    {
        const auto colour = colours::sourceColour (mod::srcEnv + i);
        auto& display = envDisplays.emplace_back (processor, i, colour);
        auto& panel = addPanel (modPage, i == 0 ? "ENV 1  (AMP)" : "ENV " + juce::String (i + 1), colour, 3);
        panel.setWidget (display, 50);
        for (size_t k = 0; k < std::size (envIds); ++k)
            panel.addKnob ("env" + juce::String (i + 1) + "_" + envIds[k], envNames[k]);
        envRow.push_back (&panel);
    }

    macroPanel = &addPanel (modPage, "MACROS", colours::cyan, 2);
    for (int i = 0; i < mod::numMacros; ++i)
        macroPanel->addKnob ("macro" + juce::String (i + 1), "MACRO " + juce::String (i + 1));
    envRow.push_back (macroPanel);

    chaosPanel = &addPanel (modPage, "CHAOS", colours::gold);
    chaosPanel->setWidget (chaosDisplay, 70);
    chaosPanel->addKnob ("chaos_rate", "LORENZ");
    chaosPanel->addKnob ("fractal_rate", "FRACTAL");
    lfoRow.push_back (chaosPanel);
    modPage.addAndMakeVisible (matrix);

    // ---- FX page
    struct FxKnob { const char* id; const char* name; };
    auto addFx = [&] (const juce::String& title, juce::Colour colour, const char* onId, const char* comboId,
                      const juce::StringArray* items, std::initializer_list<FxKnob> knobs)
    {
        auto& panel = addPanel (fxPage, title, colour);
        panel.addOnSwitch (onId);
        if (comboId != nullptr)
            panel.addCombo (comboId, *items);
        for (auto& k : knobs)
            panel.addKnob (k.id, k.name);
        fxPanels.push_back (&panel);
    };

    addFx ("DISTORTION", colours::coral, "fx_dist_on", "fx_dist_type", &fx::distTypeNames,
           { { "fx_dist_drive", "DRIVE" }, { "fx_dist_tone", "TONE" }, { "fx_dist_mix", "MIX" } });
    addFx ("CHORUS / FLANGER", colours::cyan, "fx_chorus_on", "fx_chorus_mode", &fx::chorusModeNames,
           { { "fx_chorus_rate", "RATE" }, { "fx_chorus_depth", "DEPTH" }, { "fx_chorus_feedback", "FEEDBACK" }, { "fx_chorus_mix", "MIX" } });
    addFx ("PING-PONG DELAY", colours::magenta, "fx_delay_on", nullptr, nullptr,
           { { "fx_delay_time", "TIME" }, { "fx_delay_feedback", "FEEDBACK" }, { "fx_delay_tone", "TONE" }, { "fx_delay_mix", "MIX" } });
    addFx ("REVERB", colours::gold, "fx_reverb_on", nullptr, nullptr,
           { { "fx_reverb_size", "SIZE" }, { "fx_reverb_decay", "DECAY" }, { "fx_reverb_damp", "DAMPING" }, { "fx_reverb_mix", "MIX" } });

    // ---- header: tabs + draggable sources
    for (auto* b : { &soundTab, &modTab, &fxTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        addAndMakeVisible (b);
    }
    soundTab.setToggleState (true, juce::dontSendNotification);
    soundTab.onClick = [this] { showPage (0); };
    modTab.onClick   = [this] { showPage (1); };
    fxTab.onClick    = [this] { showPage (2); };

    sourcesLabel.setText ("MOD SOURCES", juce::dontSendNotification);
    sourcesLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    sourcesLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    addAndMakeVisible (sourcesLabel);

    const auto srcNames = mod::sourceNames();
    for (int s = 1; s < mod::numSources; ++s)
    {
        chips.push_back (std::make_unique<SourceChip> (s, srcNames[s].toUpperCase(), colours::sourceColour (s)));
        addAndMakeVisible (*chips.back());
    }

    setSize (1040, 800);
}

EmberlineEditor::~EmberlineEditor() { setLookAndFeel (nullptr); }

Panel& EmberlineEditor::addPanel (juce::Component& page, const juce::String& title, juce::Colour accent, int columns)
{
    auto& panel = panels.emplace_back (processor, title, accent, columns);
    page.addAndMakeVisible (panel);
    return panel;
}

void EmberlineEditor::showPage (int page)
{
    soundPage.setVisible (page == 0);
    modPage.setVisible (page == 1);
    fxPage.setVisible (page == 2);
}

void EmberlineEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bgPanel, 0.0f, 0.0f, colours::bgDeep, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (colours::cyan);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("EMBERLINE SPECTRALIS", getLocalBounds().removeFromTop (46).reduced (20, 0), juce::Justification::centredLeft);
}

void EmberlineEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);

    auto top = area.removeFromTop (46);
    fxTab.setBounds (top.removeFromRight (70).reduced (2, 9));
    modTab.setBounds (top.removeFromRight (90).reduced (2, 9));
    soundTab.setBounds (top.removeFromRight (90).reduced (2, 9));

    auto chipRow = area.removeFromTop (30);
    sourcesLabel.setBounds (chipRow.removeFromLeft (100));
    const int chipW = juce::jmin (84, chipRow.getWidth() / (int) chips.size());
    for (auto& c : chips)
        c->setBounds (chipRow.removeFromLeft (chipW).reduced (3, 2));

    area.removeFromTop (6);
    area.removeFromBottom (12);
    soundPage.setBounds (area);
    modPage.setBounds (area);
    fxPage.setBounds (area);

    // SOUND page
    {
        auto page = soundPage.getLocalBounds();
        auto oscArea = page.removeFromTop (page.getHeight() * 62 / 100);
        page.removeFromTop (8);

        const int w = oscArea.getWidth() / (int) oscRow.size();
        for (auto* p : oscRow)
            p->setBounds (oscArea.removeFromLeft (w).reduced (4, 0));

        const float weights[] { 1.5f, 1.3f, 1.3f, 0.9f };
        float total = 0.0f;
        for (auto v : weights) total += v;
        const int fullW = page.getWidth();
        for (size_t i = 0; i < lowerRow.size(); ++i)
            lowerRow[i]->setBounds (page.removeFromLeft ((int) ((float) fullW * weights[i] / total)).reduced (4, 0));
    }

    // FX page: 2 x 2 grid, in signal order
    {
        auto page = fxPage.getLocalBounds();
        auto topRow = page.removeFromTop ((page.getHeight() - 8) / 2);
        page.removeFromTop (8);
        for (size_t i = 0; i < fxPanels.size(); ++i)
        {
            auto& row = i < 2 ? topRow : page;
            fxPanels[i]->setBounds (row.removeFromLeft (row.getWidth() / (i % 2 == 0 ? 2 : 1)).reduced (4, 0));
        }
    }

    // MOD page
    {
        auto page = modPage.getLocalBounds();
        auto lfoArea = page.removeFromTop (200);
        page.removeFromTop (8);
        auto envArea = page.removeFromTop (285);
        page.removeFromTop (8);

        auto splitRow = [] (juce::Rectangle<int> row, const std::vector<Panel*>& row_panels)
        {
            const int cellW = row.getWidth() / (int) row_panels.size();
            for (auto* p : row_panels)
                p->setBounds (row.removeFromLeft (cellW).reduced (4, 0));
        };
        splitRow (lfoArea, lfoRow);
        splitRow (envArea, envRow);

        matrix.setBounds (page.reduced (4, 0));
    }
}
