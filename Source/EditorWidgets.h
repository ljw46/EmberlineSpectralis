#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include <deque>
#include <functional>

// Popup with every modulation route on one destination: depth, curve, polarity and removal, without a trip
// to the matrix page. Opens automatically when a source chip is dropped on a knob, and on right-click.
class ModRouteEditor final : public juce::Component
{
public:
    ModRouteEditor (EmberlineProcessor& p, int destIndex) : proc (p), dest (destIndex)
    {
        title.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.06f));
        title.setColour (juce::Label::textColourId, ember::colours::magenta);
        addAndMakeVisible (title);
        rebuild();
    }

    int numRoutes() const noexcept { return (int) rows.size(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8);
        title.setBounds (r.removeFromTop (18));
        r.removeFromTop (4);
        for (auto& row : rows)
            row->setBounds (r.removeFromTop (rowH));
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0c111a)); }

private:
    static constexpr int rowH = 58, width = 380;

    class Row final : public juce::Component
    {
    public:
        Row (EmberlineProcessor& p, int slotIndex, std::function<void()> onRemove)
        {
            using A = juce::AudioProcessorValueTreeState;
            const int src = juce::roundToInt (p.modPtrs.slots[(size_t) slotIndex].src->load());
            colour = ember::colours::sourceColour (src);

            name.setText (ember::mod::sourceNames()[src], juce::dontSendNotification);
            name.setColour (juce::Label::textColourId, colour);
            name.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            for (auto* sl : { &depth, &curve })
            {
                sl->setSliderStyle (juce::Slider::LinearHorizontal);
                sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
                sl->setColour (juce::Slider::trackColourId, colour);
                sl->setColour (juce::Slider::thumbColourId, colour);
                sl->setColour (juce::Slider::textBoxOutlineColourId, colour.withAlpha (0.4f));
            }
            depthLabel.setText ("DEPTH", juce::dontSendNotification);
            curveLabel.setText ("CURVE", juce::dontSendNotification);
            for (auto* l : { &depthLabel, &curveLabel })
            {
                l->setFont (juce::FontOptions (9.5f, juce::Font::bold).withKerningFactor (0.06f));
                l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
            }
            bipolar.setButtonText ("BIPOLAR");
            bipolar.setColour (juce::ToggleButton::tickColourId, colour);
            bipolar.setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.75f));
            remove.setButtonText ("REMOVE");
            remove.onClick = [&p, slotIndex, onRemove]
            {
                for (const char* what : { "src", "dst" })
                    if (auto* prm = p.apvts.getParameter (ember::mod::slotId (slotIndex, what)))
                        prm->setValueNotifyingHost (0.0f);
                onRemove();
            };

            for (auto* c : std::initializer_list<juce::Component*> { &name, &depth, &curve, &depthLabel, &curveLabel, &bipolar, &remove })
                addAndMakeVisible (c);

            depthAtt = std::make_unique<A::SliderAttachment> (p.apvts, ember::mod::slotId (slotIndex, "depth"), depth);
            curveAtt = std::make_unique<A::SliderAttachment> (p.apvts, ember::mod::slotId (slotIndex, "curve"), curve);
            bipolarAtt = std::make_unique<A::ButtonAttachment> (p.apvts, ember::mod::slotId (slotIndex, "bipolar"), bipolar);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colour.withAlpha (0.08f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.0f, 1.0f), 5.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (0.0f, 4.0f, 3.0f, (float) getHeight() - 8.0f, 1.5f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8, 4);
            auto top = r.removeFromTop (24);
            name.setBounds (top.removeFromLeft (70));
            remove.setBounds (top.removeFromRight (64).reduced (0, 1));
            depthLabel.setBounds (top.removeFromLeft (42));
            depth.setBounds (top);
            auto bottom = r;
            bipolar.setBounds (bottom.removeFromLeft (70));
            bottom.removeFromLeft (4);
            curveLabel.setBounds (bottom.removeFromLeft (42));
            curve.setBounds (bottom);
        }

    private:
        juce::Colour colour;
        juce::Label name, depthLabel, curveLabel;
        juce::Slider depth, curve;
        juce::ToggleButton bipolar;
        juce::TextButton remove;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, curveAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bipolarAtt;
    };

    void rebuild()
    {
        rows.clear();
        for (int s = 0; s < ember::mod::numSlots; ++s)
        {
            const auto& slot = proc.modPtrs.slots[(size_t) s];
            if (juce::roundToInt (slot.src->load()) > 0 && juce::roundToInt (slot.dst->load()) - 1 == dest)
            {
                rows.push_back (std::make_unique<Row> (proc, s, [safe = juce::Component::SafePointer<ModRouteEditor> (this)]
                {
                    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refreshAfterRemove(); });
                }));
                addAndMakeVisible (*rows.back());
            }
        }

        title.setText ("MODULATING  " + ember::mod::destinations()[(size_t) dest].label.toUpperCase(), juce::dontSendNotification);
        setSize (width, 18 + 8 + 8 + 4 + juce::jmax (1, (int) rows.size()) * rowH);
    }

    void refreshAfterRemove()
    {
        rebuild();
        if (rows.empty())
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        resized();
    }

    EmberlineProcessor& proc;
    int dest;
    juce::Label title;
    std::vector<std::unique_ptr<Row>> rows;
};

// Rotary knob that accepts modulation drops and paints the routed modulation ranges around itself.
class ModKnob final : public juce::Slider,
                      public juce::DragAndDropTarget,
                      private juce::Timer
{
public:
    ModKnob (EmberlineProcessor& p, int destIndex)
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow), proc (p), dest (destIndex)
    {
        if (dest >= 0)
            startTimerHz (30);
    }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return dest >= 0 && d.description.toString().startsWith ("mod:");
    }

    void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { hover = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        hover = false;
        proc.assignModulation (d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue(), dest);
        repaint();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ModKnob> (this)] { if (safe != nullptr) safe->openRouteEditor(); });
    }

    // right-click a modulated knob to tune its routes in place
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && dest >= 0)
        {
            openRouteEditor();
            return;
        }
        juce::Slider::mouseDown (e);
    }

    void openRouteEditor()
    {
        auto* top = getTopLevelComponent();
        if (dest < 0 || top == nullptr)
            return;

        auto editor = std::make_unique<ModRouteEditor> (proc, dest);
        if (editor->numRoutes() == 0)
            return;

        juce::CallOutBox::launchAsynchronously (std::move (editor), top->getLocalArea (this, getLocalBounds()), top);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (dest < 0)
            return;

        const auto geo = ember::knobGeometry (getLookAndFeel().getSliderLayout (*this).sliderBounds);
        const auto c = geo.centre;
        const float radius = geo.radius + 2.5f;
        const auto rp = getRotaryParameters();
        auto angleOf = [&] (double prop) { return rp.startAngleRadians + (float) prop * (rp.endAngleRadians - rp.startAngleRadians); };

        if (hover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.drawEllipse (c.x - geo.radius - 3.0f, c.y - geo.radius - 3.0f, 2 * geo.radius + 6.0f, 2 * geo.radius + 6.0f, 1.5f);
        }

        const double base = valueToProportionOfLength (getValue());
        const float live = proc.modCtx.liveOffset[(size_t) dest].load (std::memory_order_relaxed);
        bool any = false;

        for (const auto& slot : proc.modPtrs.slots)
        {
            const int src = juce::roundToInt (slot.src->load());
            if (src <= 0 || juce::roundToInt (slot.dst->load()) - 1 != dest)
                continue;

            any = true;
            const double depth = slot.depth->load();
            const bool bipolar = slot.bipolar->load() > 0.5f;
            const double lo = bipolar ? juce::jlimit (0.0, 1.0, base - std::abs (depth)) : juce::jlimit (0.0, 1.0, juce::jmin (base, base + depth));
            const double hi = bipolar ? juce::jlimit (0.0, 1.0, base + std::abs (depth)) : juce::jlimit (0.0, 1.0, juce::jmax (base, base + depth));
            const auto colour = ember::colours::sourceColour (src);

            juce::Path arc;
            arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, angleOf (lo), angleOf (hi), true);
            g.setColour (colour.withAlpha (0.25f));
            g.strokePath (arc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour);
            g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (any)   // one live marker showing the summed offset
        {
            const float a = angleOf (juce::jlimit (0.0, 1.0, base + (double) live));
            g.setColour (juce::Colours::white);
            g.fillEllipse (c.x + std::sin (a) * radius - 2.5f, c.y - std::cos (a) * radius - 2.5f, 5.0f, 5.0f);
        }
    }

private:
    void timerCallback() override
    {
        bool has = false;
        for (const auto& slot : proc.modPtrs.slots)
            if (juce::roundToInt (slot.src->load()) > 0 && juce::roundToInt (slot.dst->load()) - 1 == dest)
                has = true;

        if (has || hadMod)
            repaint();
        hadMod = has;
    }

    EmberlineProcessor& proc;
    int dest;
    bool hover = false, hadMod = false;
};

// Draggable modulation source badge.
class SourceChip final : public juce::Component
{
public:
    SourceChip (int sourceIndex, const juce::String& text, juce::Colour c) : src (sourceIndex), label (text), colour (c)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colour.withAlpha (0.14f));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (colour.withAlpha (0.8f));
        g.drawRoundedRectangle (b, 6.0f, 1.2f);
        g.setColour (colour);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (label, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { dragging = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging && e.getDistanceFromDragStart() > 4)
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                container->startDragging ("mod:" + juce::String (src), this);
                dragging = true;
            }
    }

private:
    int src;
    juce::String label;
    juce::Colour colour;
    bool dragging = false;
};

// A titled container: #0E131D fill, 8px corners, soft glowing border in the accent colour.
class Panel final : public juce::Component
{
public:
    Panel (EmberlineProcessor& p, const juce::String& titleText, juce::Colour accentColour, int knobColumns = 0)
        : proc (p), accent (accentColour), knobColour (accentColour), columns (knobColumns)
    {
        title.setColour (juce::Label::textColourId, accent);
        title.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.06f));
        title.setInterceptsMouseClicks (false, false);
        setTitle (titleText);
        addAndMakeVisible (title);
    }

    // appearance
    void setTitle (const juce::String& t) { title.setText (t, juce::dontSendNotification); }
    void setCentredTitle (bool c)         { centredTitle = c; title.setJustificationType (c ? juce::Justification::centred : juce::Justification::centredLeft); }
    void setTinted (bool t)               { tinted = t; }
    void setKnobColour (juce::Colour c)   { knobColour = c; }
    void setCompactKnobs (bool c)         { compact = c; }   // value readout to the right, label above

    // header drag (used to reorder FX slots)
    std::function<void (Panel&, const juce::MouseEvent&)> onHeaderDrag;
    std::function<void (Panel&)> onHeaderDragEnd;

    void addKnob (const juce::String& paramId, const juce::String& name)
    {
        auto& k = knobs.emplace_back (proc, ember::mod::destForParam (paramId));
        k.compact = compact;
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, knobColour);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, knobColour.withAlpha (0.45f));
        k.slider.setTextBoxStyle (compact ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow, false, compact ? 58 : 64, 16);
        addAndMakeVisible (k.slider);

        k.label.setText (name, juce::dontSendNotification);
        k.label.setJustificationType (compact ? juce::Justification::centredLeft : juce::Justification::centred);
        k.label.setFont (juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.05f));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
    }

    void setKnobLabel (int index, const juce::String& text)
    {
        if (juce::isPositiveAndBelow (index, (int) knobs.size()))
            knobs[(size_t) index].label.setText (text, juce::dontSendNotification);
    }

    // replaces the entries of a combo (same order / count) without disturbing its attached parameter
    void setComboItems (int index, const juce::StringArray& items, const juce::StringArray& sections = {})
    {
        if (! juce::isPositiveAndBelow (index, (int) combos.size()))
            return;
        auto& box = combos[(size_t) index].box;
        const int selected = box.getSelectedItemIndex();
        box.clear (juce::dontSendNotification);
        fillCombo (box, items, sections);
        box.setSelectedItemIndex (selected, juce::dontSendNotification);
    }

    // relative widths of the items in the combo row, in the order they were added (default: equal)
    void setComboWeights (std::vector<float> weights) { comboWeights = std::move (weights); }

    // puts a custom component (e.g. the wavetable selector) into the combo row
    void addRowWidget (juce::Component& w)
    {
        rowItems.push_back (&w);
        addAndMakeVisible (w);
    }

    // `sections` (optional, one per item) adds a heading whenever the section changes
    void addCombo (const juce::String& paramId, const juce::StringArray& items, const juce::StringArray& sections = {})
    {
        auto& c = combos.emplace_back();
        rowItems.push_back (&c.box);
        fillCombo (c.box, items, sections);
        c.box.setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.45f));
        addAndMakeVisible (c.box);
        c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c.box);
    }

    // an on/off button in the combo row
    void addToggle (const juce::String& paramId, const juce::String& text)
    {
        auto& t = toggles.emplace_back();
        t.button.setButtonText (text);
        t.button.setColour (juce::ToggleButton::tickColourId, accent);
        t.button.setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.8f));
        rowItems.push_back (&t.button);
        addAndMakeVisible (t.button);
        t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, t.button);
    }

    void addOnSwitch (const juce::String& paramId)
    {
        onButton.setButtonText ("ON");
        onButton.setColour (juce::ToggleButton::tickColourId, accent);
        onButton.setColour (juce::ToggleButton::tickDisabledColourId, accent.withAlpha (0.3f));
        onButton.setColour (juce::ToggleButton::textColourId, accent);
        addAndMakeVisible (onButton);
        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, onButton);
        hasOn = true;
    }

    // A custom widget (graph etc.). `side` puts it left of the controls instead of above them.
    void setWidget (juce::Component& w, int height, bool side = false, float sideFraction = 0.62f)
    {
        widget = &w;
        widgetHeight = height;
        widgetOnSide = side;
        widgetSideFraction = sideFraction;
        addAndMakeVisible (w);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (ember::colours::panelFill);
        g.fillRoundedRectangle (b, 8.0f);
        if (tinted)
        {
            g.setColour (accent.withAlpha (0.06f));
            g.fillRoundedRectangle (b, 8.0f);
        }
        g.setColour (accent.withAlpha (0.1f));
        g.drawRoundedRectangle (b.expanded (1.5f), 9.0f, 3.0f);
        g.setColour (accent.withAlpha (0.8f));
        g.drawRoundedRectangle (b, 8.0f, 1.3f);

        if (onHeaderDrag)   // grip dots
        {
            g.setColour (accent.withAlpha (0.55f));
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 3; ++c)
                    g.fillEllipse (10.0f + (float) c * 5.0f, 11.0f + (float) r * 5.0f, 2.2f, 2.2f);
        }
    }

    void resized() override
    {
        auto inner = getLocalBounds().reduced (10, 8);
        auto head = inner.removeFromTop (20);
        if (hasOn)
            onButton.setBounds (head.removeFromRight (56));
        if (onHeaderDrag)
            head.removeFromLeft (22);
        title.setBounds (head);

        if (widget != nullptr)
        {
            if (widgetOnSide)
            {
                widget->setBounds (inner.removeFromLeft ((int) ((float) inner.getWidth() * widgetSideFraction)));
                inner.removeFromLeft (8);
            }
            else
            {
                widget->setBounds (inner.removeFromTop (widgetHeight));
                inner.removeFromTop (4);
            }
        }

        if (! rowItems.empty())
        {
            auto row = inner.removeFromTop (24);
            float total = 0.0f;
            for (size_t i = 0; i < rowItems.size(); ++i)
                total += i < comboWeights.size() ? comboWeights[i] : 1.0f;
            const int rowWidth = row.getWidth();
            for (size_t i = 0; i < rowItems.size(); ++i)
            {
                const float weight = i < comboWeights.size() ? comboWeights[i] : 1.0f;
                rowItems[i]->setBounds (row.removeFromLeft ((int) ((float) rowWidth * weight / total)).reduced (2, 0));
            }
            inner.removeFromTop (4);
        }

        const int n = (int) knobs.size();
        if (n == 0)
            return;

        const int cols = columns > 0 ? columns : n;
        const int rows = (n + cols - 1) / cols;
        const int rowH = inner.getHeight() / rows;

        for (int r = 0; r < rows; ++r)
        {
            auto row = inner.removeFromTop (rowH);
            const int cellW = row.getWidth() / cols;
            for (int c = 0; c < cols && r * cols + c < n; ++c)
            {
                auto cell = row.removeFromLeft (cellW);
                auto& k = knobs[(size_t) (r * cols + c)];
                if (k.compact)
                {
                    k.label.setBounds (cell.removeFromTop (13));
                    k.slider.setBounds (cell);
                }
                else
                {
                    k.label.setBounds (cell.removeFromBottom (14));
                    k.slider.setBounds (cell);
                }
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (onHeaderDrag && e.getMouseDownY() < 30)
            onHeaderDrag (*this, e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onHeaderDragEnd && e.getMouseDownY() < 30)
            onHeaderDragEnd (*this);
    }

private:
    static void fillCombo (juce::ComboBox& box, const juce::StringArray& items, const juce::StringArray& sections)
    {
        if (sections.isEmpty())
        {
            box.addItemList (items, 1);
            return;
        }
        juce::String last;
        for (int i = 0; i < items.size(); ++i)
        {
            if (sections[i] != last)
            {
                last = sections[i];
                box.addSectionHeading (last);
            }
            box.addItem (items[i], i + 1);   // ids stay index + 1 whatever headings are added
        }
    }

    struct Knob
    {
        Knob (EmberlineProcessor& p, int dest) : slider (p, dest) {}
        ModKnob slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        bool compact = false;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    EmberlineProcessor& proc;
    juce::Colour accent, knobColour;
    int columns;
    bool tinted = false, compact = false, centredTitle = false;
    juce::Label title;
    juce::ToggleButton onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    bool hasOn = false;
    std::deque<Combo> combos;
    std::deque<Toggle> toggles;
    std::deque<Knob> knobs;
    juce::Component* widget = nullptr;
    std::vector<float> comboWeights;
    std::vector<juce::Component*> rowItems;   // everything in the combo row, in order
    int widgetHeight = 0;
    bool widgetOnSide = false;
    float widgetSideFraction = 0.62f;
};
