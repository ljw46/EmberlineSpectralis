#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"

// Vector LFO editor: drag nodes, drag a segment's diamond to bend it, double-click to add/remove
// nodes, right-click for shape presets. The playhead follows the live LFO phase.
class LfoEditor final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    LfoEditor (EmberlineProcessor& p, int lfoIndex, juce::Colour c) : proc (p), index (lfoIndex), accent (c)
    {
        nodes = proc.lfoShapes.getNodes (index);
        proc.lfoShapes.addChangeListener (this);
        startTimerHz (30);
    }

    ~LfoEditor() override { proc.lfoShapes.removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (ember::colours::bgDeep);
        g.fillRoundedRectangle (area, 5.0f);

        g.setColour (accent.withAlpha (0.12f));
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine ((int) toPx (i * 0.25f, 0.0f).x, area.getY() + 4.0f, area.getBottom() - 4.0f);
        g.setColour (accent.withAlpha (0.3f));
        g.drawHorizontalLine ((int) toPx (0.0f, 0.0f).y, area.getX() + 4.0f, area.getRight() - 4.0f);

        juce::Path curve, fill;
        const int steps = juce::jmax (8, (int) plotArea().getWidth() / 2);
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const auto pt = toPx (x, ember::mod::LfoShapes::evaluate (nodes, x));
            if (i == 0) { curve.startNewSubPath (pt); fill.startNewSubPath (pt.x, toPx (0, 0).y); fill.lineTo (pt); }
            else        { curve.lineTo (pt); fill.lineTo (pt); }
        }
        fill.lineTo (toPx (1.0f, 0.0f));
        fill.closeSubPath();

        g.setColour (accent.withAlpha (0.14f));
        g.fillPath (fill);
        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // playhead
        const float phase = proc.modCtx.displayPhase[(size_t) index].load (std::memory_order_relaxed);
        const auto head = toPx (phase, ember::mod::LfoShapes::evaluate (nodes, phase));
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawVerticalLine ((int) head.x, area.getY() + 3.0f, area.getBottom() - 3.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (head.x - 3.5f, head.y - 3.5f, 7.0f, 7.0f);

        // curve handles, then nodes
        for (size_t i = 0; i + 1 < nodes.size(); ++i)
        {
            const auto h = handlePos (i);
            juce::Path d;
            d.addQuadrilateral (h.x, h.y - 4.5f, h.x + 4.5f, h.y, h.x, h.y + 4.5f, h.x - 4.5f, h.y);
            g.setColour (accent.withAlpha (hoverHandle == (int) i ? 1.0f : 0.55f));
            g.strokePath (d, juce::PathStrokeType (1.2f));
        }

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto p = toPx (nodes[i].x, nodes[i].y);
            const float r = hoverNode == (int) i ? 6.0f : 4.5f;
            g.setColour (ember::colours::bgDeep);
            g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
            g.setColour (accent);
            g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, 1.8f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int hn = findNode (e.position), hh = hn < 0 ? findHandle (e.position) : -1;
        if (hn != hoverNode || hh != hoverHandle) { hoverNode = hn; hoverHandle = hh; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override { hoverNode = hoverHandle = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragNode = dragHandle = -1;

        if (e.mods.isPopupMenu())
        {
            showPresets();
            return;
        }

        dragNode = findNode (e.position);
        if (dragNode < 0)
            dragHandle = findHandle (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragNode >= 0)
        {
            const auto v = fromPx (e.position);
            auto& n = nodes[(size_t) dragNode];
            n.y = juce::jlimit (-1.0f, 1.0f, v.y);

            if (dragNode > 0 && dragNode < (int) nodes.size() - 1)
                n.x = juce::jlimit (nodes[(size_t) dragNode - 1].x + 0.002f, nodes[(size_t) dragNode + 1].x - 0.002f, v.x);

            commit();
        }
        else if (dragHandle >= 0)
        {
            auto& a = nodes[(size_t) dragHandle];
            const auto& b = nodes[(size_t) dragHandle + 1];
            const float dy = b.y - a.y;
            if (std::abs (dy) > 0.03f)
            {
                // find the curve value whose midpoint lands under the cursor
                const float target = juce::jlimit (0.08f, 0.92f, (fromPx (e.position).y - a.y) / dy);
                float lo = -1.0f, hi = 1.0f;
                for (int i = 0; i < 20; ++i)
                {
                    const float mid = 0.5f * (lo + hi);
                    (ember::curveShape (0.5f, mid) < target ? lo : hi) = mid;
                }
                a.curve = 0.5f * (lo + hi);
                commit();
            }
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int hit = findNode (e.position);
        if (hit > 0 && hit < (int) nodes.size() - 1)
            nodes.erase (nodes.begin() + hit);
        else if (hit < 0 && (int) nodes.size() < ember::mod::maxLfoNodes)
        {
            const auto v = fromPx (e.position);
            nodes.push_back ({ juce::jlimit (0.002f, 0.998f, v.x), juce::jlimit (-1.0f, 1.0f, v.y), 0.0f });
        }
        else
            return;

        commit();
    }

private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 10.0f); }

    juce::Point<float> toPx (float x, float y) const
    {
        const auto a = plotArea();
        return { a.getX() + x * a.getWidth(), a.getY() + (1.0f - y) * 0.5f * a.getHeight() };
    }

    juce::Point<float> fromPx (juce::Point<float> p) const
    {
        const auto a = plotArea();
        return { (p.x - a.getX()) / a.getWidth(), 1.0f - 2.0f * (p.y - a.getY()) / a.getHeight() };
    }

    juce::Point<float> handlePos (size_t seg) const
    {
        const float x = 0.5f * (nodes[seg].x + nodes[seg + 1].x);
        return toPx (x, ember::mod::LfoShapes::evaluate (nodes, x));
    }

    int findNode (juce::Point<float> p) const
    {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (toPx (nodes[i].x, nodes[i].y).getDistanceFrom (p) < 8.0f)
                return (int) i;
        return -1;
    }

    int findHandle (juce::Point<float> p) const
    {
        for (size_t i = 0; i + 1 < nodes.size(); ++i)
            if (handlePos (i).getDistanceFrom (p) < 7.0f)
                return (int) i;
        return -1;
    }

    void commit() { proc.lfoShapes.setNodes (index, nodes); repaint(); }

    void showPresets()
    {
        juce::PopupMenu m;
        const char* names[] { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random Steps" };
        for (int i = 0; i < 6; ++i)
            m.addItem (i + 1, names[i]);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, safe = juce::Component::SafePointer<LfoEditor> (this)] (int r)
        {
            if (safe != nullptr && r > 0)
            {
                nodes = ember::mod::LfoShapes::presetNodes (r - 1);
                commit();
            }
        });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        nodes = proc.lfoShapes.getNodes (index);
        repaint();
    }

    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
    int index;
    juce::Colour accent;
    std::vector<ember::mod::LfoNode> nodes;
    int dragNode = -1, dragHandle = -1, hoverNode = -1, hoverHandle = -1;
};

// Read-only ADSR shape preview driven by the envelope parameters.
class EnvDisplay final : public juce::Component, private juce::Timer
{
public:
    EnvDisplay (EmberlineProcessor& p, int envIndex, juce::Colour c) : proc (p), index (envIndex), accent (c) { startTimerHz (20); }

    void paint (juce::Graphics& g) override
    {
        const auto& e = proc.modPtrs.env[(size_t) index];
        const float a = e.attack->load(), d = e.decay->load(), s = e.sustain->load(), r = e.release->load();
        const float ac = e.aCurve->load(), dc = e.dCurve->load();

        const auto area = getLocalBounds().toFloat();
        g.setColour (ember::colours::bgDeep);
        g.fillRoundedRectangle (area, 5.0f);

        const auto plot = area.reduced (8.0f, 8.0f);
        const float wa = std::sqrt (a), wd = std::sqrt (d), wr = std::sqrt (r), wh = 0.5f * (wa + wd + wr) + 0.1f;
        const float total = wa + wd + wh + wr;
        auto X = [&] (float units) { return plot.getX() + plot.getWidth() * units / total; };
        auto Y = [&] (float v) { return plot.getBottom() - v * plot.getHeight(); };

        juce::Path path;
        path.startNewSubPath (X (0.0f), Y (0.0f));
        constexpr int seg = 24;
        for (int i = 1; i <= seg; ++i)
        {
            const float t = (float) i / seg;
            path.lineTo (X (wa * t), Y (ember::curveShape (t, ac)));
        }
        for (int i = 1; i <= seg; ++i)
        {
            const float t = (float) i / seg;
            path.lineTo (X (wa + wd * t), Y (1.0f - (1.0f - s) * ember::curveShape (t, dc)));
        }
        path.lineTo (X (wa + wd + wh), Y (s));
        for (int i = 1; i <= seg; ++i)
        {
            const float t = (float) i / seg;
            path.lineTo (X (wa + wd + wh + wr * t), Y (s * (1.0f - ember::curveShape (t, 0.5f))));
        }

        auto fill = path;
        fill.lineTo (X (total), Y (0.0f));
        fill.closeSubPath();
        g.setColour (accent.withAlpha (0.14f));
        g.fillPath (fill);
        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
    int index;
    juce::Colour accent;
};

// Lists every matrix slot so routings can be re-pointed, rescaled (bipolar depth) or cleared.
class MatrixPanel final : public juce::Component
{
public:
    explicit MatrixPanel (EmberlineProcessor& p)
    {
        title.setText ("MOD MATRIX", juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, ember::colours::cyan);
        title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (title);

        for (int s = 0; s < ember::mod::numSlots; ++s)
        {
            auto& row = rows.emplace_back (p, s);
            addAndMakeVisible (row);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (ember::colours::bgDeep.withAlpha (0.55f));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (ember::colours::cyan.withAlpha (0.3f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    void resized() override
    {
        auto inner = getLocalBounds().reduced (10, 8);
        title.setBounds (inner.removeFromTop (20));
        inner.removeFromTop (2);

        const int perCol = ember::mod::numSlots / 2;
        const int rowH = inner.getHeight() / perCol;
        auto left = inner.removeFromLeft (inner.getWidth() / 2 - 6);
        inner.removeFromLeft (12);

        for (int i = 0; i < ember::mod::numSlots; ++i)
            rows[(size_t) i].setBounds ((i < perCol ? left : inner).removeFromTop (rowH).reduced (0, 2));
    }

private:
    struct Row final : juce::Component
    {
        Row (EmberlineProcessor& p, int slot) : proc (p), slotIndex (slot)
        {
            src.addItemList (ember::mod::sourceNames(), 1);
            dst.addItemList (ember::mod::destChoiceNames(), 1);
            depth.setSliderStyle (juce::Slider::LinearHorizontal);
            depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
            depth.setColour (juce::Slider::thumbColourId, ember::colours::cyan);
            depth.setColour (juce::Slider::trackColourId, ember::colours::cyan.withAlpha (0.6f));
            clear.setButtonText ("x");
            clear.onClick = [this]
            {
                for (const char* what : { "src", "dst" })
                    if (auto* prm = proc.apvts.getParameter (ember::mod::slotId (slotIndex, what)))
                        prm->setValueNotifyingHost (0.0f);
            };

            for (auto* c : std::initializer_list<juce::Component*> { &src, &dst, &depth, &clear })
                addAndMakeVisible (c);

            srcAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ember::mod::slotId (slot, "src"), src);
            dstAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ember::mod::slotId (slot, "dst"), dst);
            depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ember::mod::slotId (slot, "depth"), depth);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            clear.setBounds (r.removeFromRight (22));
            r.removeFromRight (4);
            src.setBounds (r.removeFromLeft (r.getWidth() * 29 / 100));
            r.removeFromLeft (4);
            dst.setBounds (r.removeFromLeft (r.getWidth() * 46 / 100));
            r.removeFromLeft (4);
            depth.setBounds (r);
        }

        EmberlineProcessor& proc;
        int slotIndex;
        juce::ComboBox src, dst;
        juce::Slider depth;
        juce::TextButton clear;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, dstAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt;
    };

    juce::Label title;
    std::deque<Row> rows;
};

// Lorenz butterfly: fading trail of the attractor's x/z position.
class ChaosDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit ChaosDisplay (EmberlineProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (ember::colours::bgDeep);
        g.fillRoundedRectangle (area, 5.0f);

        const auto plot = area.reduced (8.0f);
        constexpr int n = ember::mod::ModContext::trailSize;
        const int head = proc.modCtx.trailHead.load (std::memory_order_relaxed);

        auto point = [&] (int age)   // age 0 = newest
        {
            const size_t idx = (size_t) ((head - 1 - age + 2 * n) % n);
            const float x = proc.modCtx.trailX[idx].load (std::memory_order_relaxed);
            const float z = proc.modCtx.trailZ[idx].load (std::memory_order_relaxed);
            return juce::Point<float> (plot.getCentreX() + x * 0.5f * plot.getWidth(), plot.getCentreY() - z * 0.5f * plot.getHeight());
        };

        for (int age = n - 2; age >= 0; --age)
        {
            const float fade = 1.0f - (float) age / (float) n;
            g.setColour (ember::colours::gold.withAlpha (0.08f + 0.8f * fade * fade));
            g.drawLine ({ point (age + 1), point (age) }, 1.0f + 1.2f * fade);
        }

        const auto head2 = point (0);
        g.setColour (ember::colours::gold.withAlpha (0.35f));
        g.fillEllipse (head2.x - 6.0f, head2.y - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (head2.x - 2.5f, head2.y - 2.5f, 5.0f, 5.0f);
    }

private:
    void timerCallback() override { repaint(); }

    EmberlineProcessor& proc;
};
