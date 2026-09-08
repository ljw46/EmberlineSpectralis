#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include <deque>
#include <functional>

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

    void addCombo (const juce::String& paramId, const juce::StringArray& items)
    {
        auto& c = combos.emplace_back();
        c.box.addItemList (items, 1);
        c.box.setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.45f));
        addAndMakeVisible (c.box);
        c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c.box);
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

        if (! combos.empty())
        {
            auto row = inner.removeFromTop (24);
            const int w = row.getWidth() / (int) combos.size();
            for (auto& c : combos)
                c.box.setBounds (row.removeFromLeft (w).reduced (2, 0));
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
    struct Knob
    {
        Knob (EmberlineProcessor& p, int dest) : slider (p, dest) {}
        ModKnob slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        bool compact = false;
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
    std::deque<Knob> knobs;
    juce::Component* widget = nullptr;
    int widgetHeight = 0;
    bool widgetOnSide = false;
    float widgetSideFraction = 0.62f;
};
