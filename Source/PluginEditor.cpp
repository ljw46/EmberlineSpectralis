#include "PluginEditor.h"
#include "PhoenixLogo.h"

using namespace ember;

namespace
{
    constexpr int headerH = 54, chipH = 28, topH = 336, tabH = 34;
    const char* tabNames[] { "[ VISUALIZERS ]", "[ LFOs & CHAOS ]", "[ MOD MATRIX ]", "[ FX CHAIN ]", "[ FILTERS & ENVS ]" };
}

EmberlineEditor::EmberlineEditor (EmberlineProcessor& p)
    : AudioProcessorEditor (&p), processor (p), phoenix (ember::loadPhoenix()), mesh (p), presetBar (p), matrix (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < numPages; ++i)
    {
        addChildComponent (pages[(size_t) i]);
        tabs[(size_t) i].setButtonText (tabNames[i]);
        tabs[(size_t) i].setClickingTogglesState (true);
        tabs[(size_t) i].setRadioGroupId (1);
        tabs[(size_t) i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[(size_t) i]);
    }

    for (int i = 0; i < ember::fx::numEffects; ++i)
        fxSlot[(size_t) i] = processor.apvts.getRawParameterValue ("fx_slot" + juce::String (i + 1));

    addAndMakeVisible (presetBar);
    buildTop();
    buildVisualizers();
    buildLfosAndChaos();
    pages[matrixPage].addAndMakeVisible (matrix);
    buildFx();
    buildFiltersAndEnvs();

    // mod sources: always visible so any knob can be a drop target from any tab
    const auto srcNames = mod::sourceNames();
    for (int s = 1; s < mod::numSources; ++s)
    {
        chips.push_back (std::make_unique<SourceChip> (s, srcNames[s].toUpperCase(), colours::sourceColour (s)));
        addAndMakeVisible (*chips.back());
    }

    tabs[visualizers].setToggleState (true, juce::dontSendNotification);
    showPage (visualizers);
    setSize (1040, 874);
    timerCallback();   // macro labels etc. are right from the first frame
    startTimerHz (10);
}

EmberlineEditor::~EmberlineEditor() { setLookAndFeel (nullptr); }

Panel& EmberlineEditor::addPanel (juce::Component& parent, const juce::String& title, juce::Colour accent, int columns)
{
    auto& panel = panels.emplace_back (processor, title, accent, columns);
    parent.addAndMakeVisible (panel);
    return panel;
}

void EmberlineEditor::showPage (int page)
{
    for (int i = 0; i < numPages; ++i)
        pages[(size_t) i].setVisible (i == page);
}

// ---------------------------------------------------------------- fixed top region
void EmberlineEditor::buildTop()
{
    const juce::Colour accents[] { colours::cyan, colours::coral, colours::purple };
    const char* ids[]   { "frame", "warp_amt", "level", "unison", "detune", "spread", "semi", "fine", "pan" };
    const char* names[] { "FRAME", "WARP", "LEVEL", "UNISON", "DETUNE", "SPREAD", "SEMI", "FINE", "PAN" };

    for (int o = 0; o < params::numOscs; ++o)
    {
        auto& panel = addPanel (*this, "OSC " + juce::String (o + 1), accents[o], 3);
        panel.setCentredTitle (true);
        panel.setTinted (true);
        panel.addOnSwitch (params::osc (o, "on"));
        panel.addCombo (params::osc (o, "warp_mode"), params::warpModeNames);
        for (size_t k = 0; k < std::size (ids); ++k)
            panel.addKnob (params::osc (o, ids[k]), names[k]);
        oscPanels.push_back (&panel);
    }

    masterPanel = &addPanel (*this, "MASTER", colours::steel, 3);
    masterPanel->setCentredTitle (true);
    masterPanel->setKnobColour (colours::steel);
    masterPanel->addKnob (params::filt (0, "cutoff"), "CUTOFF");
    masterPanel->addKnob (params::filt (0, "res"), "RESONANCE");
    masterPanel->addKnob (params::masterVolume, "VOLUME");

    macroPanel = &addPanel (*this, "PERFORMANCE MACROS", colours::amber, 2);
    macroPanel->setCentredTitle (true);
    for (int i = 0; i < mod::numMacros; ++i)
        macroPanel->addKnob ("macro" + juce::String (i + 1), "M" + juce::String (i + 1));
}

// ---------------------------------------------------------------- tab 1
void EmberlineEditor::buildVisualizers()
{
    auto& page = pages[visualizers];
    page.addAndMakeVisible (mesh);
    for (int i = 0; i < mod::numLfos; ++i)
        page.addAndMakeVisible (lfoPreviews.emplace_back (processor, i, i == 0 ? colours::cyan : i == 1 ? colours::coral : colours::purple));
    page.addAndMakeVisible (chaosDisplays.emplace_back (processor));
}

void EmberlineEditor::layoutVisualizers()
{
    auto b = pages[visualizers].getLocalBounds().reduced (10);
    const int side = b.getWidth() * 22 / 100;
    auto left = b.removeFromLeft (side);
    auto right = b.removeFromRight (side);
    b.reduce (6, 0);

    const int halfL = (left.getHeight() - 8) / 2;
    lfoPreviews[0].setBounds (left.removeFromTop (halfL));
    left.removeFromTop (8);
    lfoPreviews[1].setBounds (left);

    lfoPreviews[2].setBounds (right.removeFromTop (halfL));
    right.removeFromTop (8);
    chaosDisplays[0].setBounds (right);

    mesh.setBounds (b);
}

// ---------------------------------------------------------------- tab 2
void EmberlineEditor::buildLfosAndChaos()
{
    auto& page = pages[lfosAndChaos];
    for (int i = 0; i < mod::numLfos; ++i)
    {
        auto& editor = lfoEditors.emplace_back (processor, i, colours::magenta);
        auto& panel = addPanel (page, "LFO " + juce::String (i + 1), colours::magenta);
        panel.setWidget (editor, 0, true, 0.64f);
        panel.addCombo ("lfo" + juce::String (i + 1) + "_mode", mod::lfoModeNames);
        panel.addCombo ("lfo" + juce::String (i + 1) + "_sync", tempo::names());
        panel.setCompactKnobs (true);
        panel.addKnob ("lfo" + juce::String (i + 1) + "_rate", "RATE");
        lfoPanels.push_back (&panel);
    }

    chaosPanel = &addPanel (page, "LORENZ CHAOS ENGINE", colours::magenta);
    chaosPanel->setWidget (chaosDisplays.emplace_back (processor), 250);
    chaosPanel->addKnob ("chaos_rate", "LORENZ RATE");
    chaosPanel->addKnob ("fractal_rate", "FRACTAL RATE");
}

void EmberlineEditor::layoutLfosAndChaos()
{
    auto b = pages[lfosAndChaos].getLocalBounds().reduced (10);
    auto left = b.removeFromLeft (b.getWidth() * 62 / 100);
    b.removeFromLeft (8);
    chaosPanel->setBounds (b);

    const int h = (left.getHeight() - 16) / 3;
    for (auto* p : lfoPanels)
    {
        p->setBounds (left.removeFromTop (h));
        left.removeFromTop (8);
    }
}

// ---------------------------------------------------------------- tab 4
void EmberlineEditor::buildFx()
{
    auto& page = pages[fxChain];

    struct Spec { const char* title; juce::Colour colour; const char* on; const char* combo; const juce::StringArray* items;
                  std::initializer_list<std::pair<const char*, const char*>> knobs; };
    const Spec specs[] {
        { "DISTORTION", colours::coral, "fx_dist_on", "fx_dist_type", &fx::distTypeNames,
          { { "fx_dist_drive", "DRIVE" }, { "fx_dist_tone", "TONE" }, { "fx_dist_mix", "MIX" } } },
        { "CHORUS", colours::cyan, "fx_chorus_on", "fx_chorus_mode", &fx::chorusModeNames,
          { { "fx_chorus_rate", "RATE" }, { "fx_chorus_depth", "DEPTH" }, { "fx_chorus_feedback", "FEEDBACK" }, { "fx_chorus_mix", "MIX" } } },
        { "PING-PONG DELAY", colours::purple, "fx_delay_on", "fx_delay_sync", &tempo::names(),
          { { "fx_delay_time", "TIME" }, { "fx_delay_feedback", "FEEDBACK" }, { "fx_delay_tone", "TONE" }, { "fx_delay_mix", "MIX" } } },
        { "REVERB", colours::gold, "fx_reverb_on", nullptr, nullptr,
          { { "fx_reverb_size", "SIZE" }, { "fx_reverb_decay", "DECAY" }, { "fx_reverb_damp", "DAMPING" }, { "fx_reverb_mix", "MIX" } } },
    };

    for (int e = 0; e < fx::numEffects; ++e)
    {
        const auto& s = specs[e];
        auto& panel = addPanel (page, s.title, s.colour, 2);
        panel.addOnSwitch (s.on);
        if (s.combo != nullptr)
            panel.addCombo (s.combo, *s.items);
        for (auto& k : s.knobs)
            panel.addKnob (k.first, k.second);

        panel.onHeaderDrag = [this] (Panel& pnl, const juce::MouseEvent& ev) { fxDrag (pnl, ev); };
        fxPanels[(size_t) e] = &panel;
    }

    fxHint.setText ("SIGNAL FLOW  >>  drag a panel header left or right to reorder the chain", juce::dontSendNotification);
    fxHint.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.06f));
    fxHint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    page.addAndMakeVisible (fxHint);
}

std::array<int, ember::fx::numEffects> EmberlineEditor::readFxOrder() const
{
    std::array<bool, fx::numEffects> used {};
    std::array<int, fx::numEffects> order {};
    int n = 0;
    for (auto* s : fxSlot)
    {
        const int e = juce::jlimit (0, fx::numEffects - 1, juce::roundToInt (s->load()));
        if (! used[(size_t) e]) { used[(size_t) e] = true; order[(size_t) n++] = e; }
    }
    for (int e = 0; e < fx::numEffects; ++e)
        if (! used[(size_t) e]) order[(size_t) n++] = e;
    return order;
}

void EmberlineEditor::layoutFx()
{
    auto b = pages[fxChain].getLocalBounds().reduced (10);
    fxHint.setBounds (b.removeFromTop (18));
    b.removeFromTop (4);

    lastFxOrder = readFxOrder();
    const int w = b.getWidth() / fx::numEffects;
    for (int slot = 0; slot < fx::numEffects; ++slot)
    {
        auto* p = fxPanels[(size_t) lastFxOrder[(size_t) slot]];
        static const char* shortNames[] { "DISTORTION", "CHORUS", "DELAY", "REVERB" };
        p->setTitle (juce::String (slot + 1) + "  " + shortNames[lastFxOrder[(size_t) slot]]);
        p->setBounds (b.removeFromLeft (w).reduced (4, 0));
    }
}

void EmberlineEditor::fxDrag (Panel& panel, const juce::MouseEvent& e)
{
    int effect = 0;
    for (int i = 0; i < fx::numEffects; ++i)
        if (fxPanels[(size_t) i] == &panel)
            effect = i;

    const auto order = readFxOrder();
    const int slotW = juce::jmax (1, pages[fxChain].getWidth() / fx::numEffects);
    const int target = juce::jlimit (0, fx::numEffects - 1, e.getEventRelativeTo (&pages[fxChain]).x / slotW);

    int current = 0;
    for (int s = 0; s < fx::numEffects; ++s)
        if (order[(size_t) s] == effect)
            current = s;

    if (target == current)
        return;

    auto set = [this] (int slot, int value)
    {
        if (auto* prm = processor.apvts.getParameter ("fx_slot" + juce::String (slot + 1)))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) value));
    };

    set (current, order[(size_t) target]);   // swap the two slots
    set (target, effect);
    layoutFx();
}

// ---------------------------------------------------------------- tab 5
void EmberlineEditor::buildFiltersAndEnvs()
{
    auto& page = pages[filtersAndEnvs];

    auto& sub = addPanel (page, "SUB / NOISE", colours::coral);
    sub.addCombo ("sub_wave", params::subWaveNames);
    sub.addCombo ("sub_oct", params::subOctNames);
    sub.addCombo ("noise_type", params::noiseTypeNames);
    sub.addKnob ("sub_level", "SUB");
    sub.addKnob ("noise_level", "NOISE");
    filterRow.push_back (&sub);

    for (int f = 0; f < 2; ++f)
    {
        auto& panel = addPanel (page, "FILTER " + juce::String (f + 1), f == 0 ? colours::cyan : colours::purple);
        panel.addCombo (params::filt (f, "type"), params::filterTypeNames);
        panel.addKnob (params::filt (f, "cutoff"), "CUTOFF");
        panel.addKnob (params::filt (f, "res"), "RESONANCE");
        panel.addKnob (params::filt (f, "drive"), "DRIVE");
        filterRow.push_back (&panel);
    }

    auto& routing = addPanel (page, "ROUTING / TEMPO", colours::steel);
    routing.addCombo ("filt_routing", params::routingNames);
    routing.addKnob ("global_tempo", "TEMPO (NO HOST)");
    filterRow.push_back (&routing);

    const char* envIds[]   { "attack", "decay", "sustain", "release", "acurve", "dcurve" };
    const char* envNames[] { "ATK", "DEC", "SUS", "REL", "A CRV", "D CRV" };
    for (int i = 0; i < mod::numEnvs; ++i)
    {
        auto& display = envDisplays.emplace_back (processor, i, colours::coral);
        auto& panel = addPanel (page, i == 0 ? "ENV 1  (AMP)" : "ENV " + juce::String (i + 1), colours::magenta);
        panel.setWidget (display, 46);
        for (size_t k = 0; k < std::size (envIds); ++k)
            panel.addKnob ("env" + juce::String (i + 1) + "_" + envIds[k], envNames[k]);
        envPanels.push_back (&panel);
    }
}

void EmberlineEditor::layoutFiltersAndEnvs()
{
    auto b = pages[filtersAndEnvs].getLocalBounds().reduced (10);
    auto row1 = b.removeFromTop (168);
    b.removeFromTop (8);

    const float weights[] { 1.15f, 1.3f, 1.3f, 0.75f };
    float total = 0.0f;
    for (auto v : weights) total += v;
    const int fullW = row1.getWidth();
    for (size_t i = 0; i < filterRow.size(); ++i)
        filterRow[i]->setBounds (row1.removeFromLeft ((int) ((float) fullW * weights[i] / total)).reduced (3, 0));

    const int w = b.getWidth() / (int) envPanels.size();
    for (auto* p : envPanels)
        p->setBounds (b.removeFromLeft (w).reduced (3, 0));
}

void EmberlineEditor::timerCallback()
{
    const auto names = processor.getMacroNames();
    if (names != shownMacroNames)
    {
        shownMacroNames = names;
        const auto parts = juce::StringArray::fromTokens (names, "|", "");
        for (int i = 0; i < mod::numMacros; ++i)
            macroPanel->setKnobLabel (i, i < parts.size() && parts[i].isNotEmpty() ? parts[i].toUpperCase() : "M" + juce::String (i + 1));
    }

    if (readFxOrder() != lastFxOrder)
        layoutFx();
}

// ---------------------------------------------------------------- painting + layout
void EmberlineEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bgPanel, 0.0f, 0.0f, colours::bgDeep, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // faint horizon grid in the lower corners, echoing the cover art
    g.setColour (colours::cyan.withAlpha (0.035f));
    for (int i = 0; i < 12; ++i)
        g.drawHorizontalLine (getHeight() - 8 - i * i * 2, 0.0f, (float) getWidth());

    // header: gradient-glow title with the phoenix mark on both sides
    const juce::String title = "EMBERLINE SPECTRALIS";
    juce::GlyphArrangement ga;
    const juce::Font font (juce::FontOptions (31.0f, juce::Font::bold).withKerningFactor (0.14f));
    ga.addLineOfText (font, title, 0.0f, 0.0f);
    const auto head = getLocalBounds().removeFromTop (headerH).toFloat();

    juce::Path text;
    ga.createPath (text);
    const auto raw = text.getBounds();
    text.applyTransform (juce::AffineTransform::translation (head.getCentreX() - raw.getCentreX(), head.getCentreY() + 2.0f - raw.getCentreY()));
    const auto tb = text.getBounds();
    juce::ColourGradient grad (colours::cyan, tb.getX(), 0.0f, colours::coral, tb.getRight(), 0.0f, false);
    grad.addColour (0.5, colours::cyan.interpolatedWith (colours::magenta, 0.55f));   // avoids a grey midpoint

    for (float width : { 11.0f, 7.0f, 3.5f })
    {
        auto glow = grad;
        glow.multiplyOpacity (width > 10.0f ? 0.07f : width > 5.0f ? 0.11f : 0.18f);
        g.setGradientFill (glow);
        g.strokePath (text, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setGradientFill (grad);
    g.fillPath (text);

    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> (16.0f, 6.0f, 42.0f, 42.0f), 0.55f);
    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> ((float) getWidth() - 58.0f, 6.0f, 42.0f, 42.0f), 0.55f);

    // small marks flanking the tab bar
    const auto tabRow = tabs[0].getBounds().getUnion (tabs[numPages - 1].getBounds());
    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> (16.0f, (float) tabRow.getY() - 2.0f, 34.0f, 34.0f), 0.4f);
    ember::drawPhoenix (g, phoenix, juce::Rectangle<float> ((float) getWidth() - 50.0f, (float) tabRow.getY() - 2.0f, 34.0f, 34.0f), 0.4f);

    // tab stage frame: cyan -> purple gradient outline
    const auto f = contentFrame.toFloat().reduced (1.0f);
    g.setColour (colours::panelFill.withAlpha (0.6f));
    g.fillRoundedRectangle (f, 10.0f);
    juce::ColourGradient frame (colours::cyan, f.getX(), 0.0f, colours::purple, f.getRight(), 0.0f, false);
    auto soft = frame;
    soft.multiplyOpacity (0.15f);
    g.setGradientFill (soft);
    g.drawRoundedRectangle (f.expanded (1.5f), 11.0f, 3.5f);
    g.setGradientFill (frame);
    g.drawRoundedRectangle (f, 10.0f, 1.4f);
}

void EmberlineEditor::resized()
{
    presetBar.setBounds (66, 13, 222, 28);

    auto area = getLocalBounds().reduced (14, 0);
    area.removeFromTop (headerH);

    auto chipRow = area.removeFromTop (chipH);
    const int chipW = juce::jmin (88, chipRow.getWidth() / juce::jmax (1, (int) chips.size()));
    chipRow = chipRow.withSizeKeepingCentre (chipW * (int) chips.size(), chipH);
    for (auto& c : chips)
        c->setBounds (chipRow.removeFromLeft (chipW).reduced (3, 2));
    area.removeFromTop (4);

    // fixed top region: three oscillators + master/macros column
    auto top = area.removeFromTop (topH);
    const int oscW = top.getWidth() * 245 / 1000;
    for (auto* p : oscPanels)
        p->setBounds (top.removeFromLeft (oscW).reduced (3, 0));

    auto right = top.reduced (3, 0);
    masterPanel->setBounds (right.removeFromTop (124));
    right.removeFromTop (6);
    macroPanel->setBounds (right);

    area.removeFromTop (8);

    // tab bar
    auto tabRow = area.removeFromTop (tabH);
    const int tabW = 178;
    tabRow = tabRow.withSizeKeepingCentre (tabW * numPages, tabH);
    for (auto& t : tabs)
        t.setBounds (tabRow.removeFromLeft (tabW).reduced (4, 1));

    area.removeFromTop (6);
    area.removeFromBottom (12);
    contentFrame = area;
    for (auto& p : pages)
        p.setBounds (area);

    layoutVisualizers();
    layoutLfosAndChaos();
    matrix.setBounds (pages[matrixPage].getLocalBounds().reduced (10));
    layoutFx();
    layoutFiltersAndEnvs();
}
