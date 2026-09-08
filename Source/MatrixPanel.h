#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include <deque>

// Full-page modulation table: Source | Destination | Depth | Curve | Bipolar. Rows accept dropped
// source chips, show the route's colour, and a live bar with the destination's current modulation.
class MatrixPage final : public juce::Component
{
public:
    explicit MatrixPage (EmberlineProcessor& p)
    {
        for (const char* h : { "SOURCE", "DESTINATION", "DEPTH", "CURVE", "BIPOLAR" })
        {
            auto& l = headers.emplace_back();
            l.setText (h, juce::dontSendNotification);
            l.setFont (juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.08f));
            l.setColour (juce::Label::textColourId, ember::colours::magenta.withAlpha (0.9f));
            l.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (l);
        }

        for (int s = 0; s < ember::mod::numSlots; ++s)
            addAndMakeVisible (rows.emplace_back (p, s));
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (ember::colours::panelFill);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (ember::colours::magenta.withAlpha (0.1f));
        g.drawRoundedRectangle (b.expanded (1.5f), 9.0f, 3.0f);
        g.setColour (ember::colours::magenta.withAlpha (0.7f));
        g.drawRoundedRectangle (b, 8.0f, 1.3f);
    }

    void resized() override
    {
        auto inner = getLocalBounds().reduced (12, 8);
        auto head = inner.removeFromTop (18);
        const auto cols = columns (inner.getWidth());
        head.removeFromLeft (cols.indicator + 6);
        int i = 0;
        for (auto w : { cols.src, cols.dst, cols.depth, cols.curve, cols.bipolar })
        {
            headers[(size_t) i++].setBounds (head.removeFromLeft (w + 6));
        }

        const int rowH = inner.getHeight() / ember::mod::numSlots;
        for (auto& r : rows)
            r.setBounds (inner.removeFromTop (rowH).reduced (0, 1));
    }

    struct Columns { int indicator, src, dst, depth, curve, bipolar, clear; };
    static Columns columns (int width)
    {
        const int indicator = 44, clear = 26, bipolar = 62;
        const int rest = width - indicator - clear - bipolar - 6 * 5;
        return { indicator, rest * 17 / 100, rest * 25 / 100, rest * 29 / 100, rest * 29 / 100, bipolar, clear };
    }

private:
    class Row final : public juce::Component,
                      public juce::DragAndDropTarget,
                      private juce::Timer
    {
    public:
        Row (EmberlineProcessor& p, int slot) : proc (p), slotIndex (slot)
        {
            src.addItemList (ember::mod::sourceNames(), 1);
            dst.addItemList (ember::mod::destChoiceNames(), 1);

            for (auto* s : { &depth, &curve })
            {
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
                s->setColour (juce::Slider::textBoxOutlineColourId, ember::colours::magenta.withAlpha (0.4f));
            }
            bipolar.setButtonText ("");
            bipolar.setColour (juce::ToggleButton::tickColourId, ember::colours::magenta);
            clear.setButtonText ("x");
            clear.onClick = [this]
            {
                for (const char* what : { "src", "dst" })
                    if (auto* prm = proc.apvts.getParameter (ember::mod::slotId (slotIndex, what)))
                        prm->setValueNotifyingHost (0.0f);
            };

            for (auto* c : std::initializer_list<juce::Component*> { &src, &dst, &depth, &curve, &bipolar, &clear })
                addAndMakeVisible (c);

            using A = juce::AudioProcessorValueTreeState;
            srcAtt = std::make_unique<A::ComboBoxAttachment> (proc.apvts, ember::mod::slotId (slot, "src"), src);
            dstAtt = std::make_unique<A::ComboBoxAttachment> (proc.apvts, ember::mod::slotId (slot, "dst"), dst);
            depthAtt = std::make_unique<A::SliderAttachment> (proc.apvts, ember::mod::slotId (slot, "depth"), depth);
            curveAtt = std::make_unique<A::SliderAttachment> (proc.apvts, ember::mod::slotId (slot, "curve"), curve);
            bipolarAtt = std::make_unique<A::ButtonAttachment> (proc.apvts, ember::mod::slotId (slot, "bipolar"), bipolar);
            startTimerHz (30);
        }

        bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:"); }
        void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
        void itemDragExit (const SourceDetails&) override  { hover = false; repaint(); }

        void itemDropped (const SourceDetails& d) override
        {
            hover = false;
            const int s = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
            if (auto* p = proc.apvts.getParameter (ember::mod::slotId (slotIndex, "src")))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) s));
            if (auto* p = proc.apvts.getParameter (ember::mod::slotId (slotIndex, "bipolar")))
                p->setValueNotifyingHost (ember::mod::isBipolar (s) ? 1.0f : 0.0f);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const int s = juce::roundToInt (proc.modPtrs.slots[(size_t) slotIndex].src->load());
            const int d = juce::roundToInt (proc.modPtrs.slots[(size_t) slotIndex].dst->load()) - 1;
            const auto colour = s > 0 ? ember::colours::sourceColour (s) : juce::Colour (0xff2b3548);

            g.setColour (hover ? colour.withAlpha (0.2f) : juce::Colour (0xff0a0e15));
            g.fillRoundedRectangle (b, 5.0f);
            if (hover)
            {
                g.setColour (colour);
                g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.2f);
            }

            // route indicator: source dot + live modulation bar for the destination
            const auto cols = columns (getWidth() - 0);
            const float cy = b.getCentreY();
            g.setColour (colour.withAlpha (s > 0 ? 0.3f : 0.0f));
            g.fillEllipse (8.0f, cy - 7.0f, 14.0f, 14.0f);
            g.setColour (colour);
            g.fillEllipse (11.0f, cy - 4.0f, 8.0f, 8.0f);

            const float bx = 26.0f, bw = (float) cols.indicator - 12.0f;
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (bx, cy - 2.0f, bw, 4.0f);
            if (s > 0 && d >= 0)
            {
                const float live = juce::jlimit (-1.0f, 1.0f, proc.modCtx.liveOffset[(size_t) d].load (std::memory_order_relaxed));
                g.setColour (colour);
                g.fillRect (bx + bw * 0.5f + juce::jmin (0.0f, live * bw * 0.5f), cy - 2.0f, std::abs (live) * bw * 0.5f, 4.0f);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 2);
            const auto cols = columns (getWidth());
            r.removeFromLeft (cols.indicator);
            src.setBounds (r.removeFromLeft (cols.src));      r.removeFromLeft (6);
            dst.setBounds (r.removeFromLeft (cols.dst));      r.removeFromLeft (6);
            depth.setBounds (r.removeFromLeft (cols.depth));  r.removeFromLeft (6);
            curve.setBounds (r.removeFromLeft (cols.curve));  r.removeFromLeft (6);
            bipolar.setBounds (r.removeFromLeft (cols.bipolar).withSizeKeepingCentre (24, 24)); r.removeFromLeft (6);
            clear.setBounds (r.removeFromLeft (cols.clear));
        }

    private:
        void timerCallback() override { repaint (0, 0, 44, getHeight()); }

        EmberlineProcessor& proc;
        int slotIndex;
        bool hover = false;
        juce::ComboBox src, dst;
        juce::Slider depth, curve;
        juce::ToggleButton bipolar;
        juce::TextButton clear;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, dstAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, curveAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bipolarAtt;
    };

    std::deque<juce::Label> headers;
    std::deque<Row> rows;
};
