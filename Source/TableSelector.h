#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include "WavetableBank.h"
#include <array>

// The wavetable chooser of one oscillator: a button showing the current table's name, arrows and mouse-wheel to
// step, and a popup menu with every table - grouped by category, each with a thumbnail (first / middle / last
// frame) - plus Import / Rescan / Open folder for user wavetables.
class TableSelector final : public juce::Component, private juce::SettableTooltipClient
{
public:
    TableSelector (EmberlineProcessor& p, int oscIndex, juce::Colour accentColour)
        : proc (p), accent (accentColour),
          attachment (*proc.apvts.getParameter (ember::params::osc (oscIndex, "table")),
                      [this] (float v) { current = juce::roundToInt (v); repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff0a0e15));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (accent.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        auto inner = getLocalBounds().reduced (3, 2);
        auto left = inner.removeFromLeft (14), right = inner.removeFromRight (14);
        g.setColour (accent.withAlpha (0.9f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("<", left, juce::Justification::centred);
        g.drawText (">", right, juce::Justification::centred);

        auto& bank = proc.getWavetableBank();
        const int index = juce::jlimit (0, bank.numSlots() - 1, current);

        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (bank.displayNames()[index], inner.reduced (2, 0), juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int w = getWidth();
        if (e.x < 17)            step (-1);
        else if (e.x > w - 17)   step (+1);
        else                     showMenu();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& d) override
    {
        if (std::abs (d.deltaY) > 0.0f)
            step (d.deltaY > 0.0f ? -1 : 1);
    }

    // a small three-frame overlay of a table (first / middle / last frame, faint to bright)
    static void drawThumbnail (juce::Graphics& g, ember::WavetableBank& bank, int index, juce::Rectangle<float> area, juce::Colour colour, float alpha)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (area, 3.0f);
        const int points = 40;
        const int frames[3] { 0, ember::WavetableBank::numFrames / 2, ember::WavetableBank::numFrames - 1 };
        for (int k = 0; k < 3; ++k)
        {
            const float* t = bank.table (index, 0, frames[k]);
            juce::Path path;
            for (int i = 0; i < points; ++i)
            {
                const float v = juce::jlimit (-1.0f, 1.0f, t[i * (ember::WavetableBank::tableSize / points)]);
                const float x = area.getX() + 1.5f + (area.getWidth() - 3.0f) * (float) i / (float) (points - 1);
                const float y = area.getCentreY() - v * (area.getHeight() * 0.42f);
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            g.setColour (colour.withAlpha (alpha * (k == 0 ? 0.45f : k == 1 ? 0.7f : 1.0f)));
            g.strokePath (path, juce::PathStrokeType (k == 2 ? 1.3f : 1.0f));
        }
    }

private:
    class MenuItem final : public juce::PopupMenu::CustomComponent
    {
    public:
        MenuItem (ember::WavetableBank& b, int idx, juce::String n, juce::Colour c, bool isCurrent)
            : bank (b), index (idx), name (std::move (n)), colour (c), selected (isCurrent) {}

        void getIdealSize (int& w, int& h) override { w = 270; h = 28; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            if (isItemHighlighted())
            {
                g.setColour (colour.withAlpha (0.22f));
                g.fillRoundedRectangle (r, 4.0f);
            }
            g.setColour (selected ? colour : juce::Colours::white.withAlpha (0.88f));
            g.setFont (juce::FontOptions (13.0f, selected ? juce::Font::bold : juce::Font::plain));
            g.drawText (name, getLocalBounds().reduced (10, 0).withTrimmedRight (96), juce::Justification::centredLeft, true);
            drawThumbnail (g, bank, index, getLocalBounds().toFloat().removeFromRight (92.0f).reduced (6.0f, 3.0f), colour, 1.0f);
        }

    private:
        ember::WavetableBank& bank;
        int index;
        juce::String name;
        juce::Colour colour;
        bool selected;
    };

    void select (int index)
    {
        auto& bank = proc.getWavetableBank();
        if (juce::isPositiveAndBelow (index, bank.numSlots()))
            attachment.setValueAsCompleteGesture ((float) index);
    }

    // next / previous table; empty user slots are skipped
    void step (int dir)
    {
        auto& bank = proc.getWavetableBank();
        const int n = bank.numSlots();
        int i = current;
        for (int tries = 0; tries < n; ++tries)
        {
            i = (i + dir + n) % n;
            if (i < bank.numFactoryTables() || bank.info (i).loaded)
                break;
        }
        select (i);
    }

    void showMenu()
    {
        auto& bank = proc.getWavetableBank();
        const auto names = bank.displayNames();
        const auto categories = bank.displayCategories();

        juce::PopupMenu menu;
        juce::String last;
        for (int i = 0; i < bank.numSlots(); ++i)
        {
            const bool user = i >= bank.numFactoryTables();
            if (user && ! bank.info (i).loaded)
                continue;   // empty user slots are not offered

            if (categories[i] != last)
            {
                last = categories[i];
                menu.addSectionHeader (last == "User" ? "USER" : last.toUpperCase());
            }

            juce::PopupMenu::Item item (names[i]);
            item.itemID = i + 1;
            item.isTicked = false;
            item.setCustomComponent (new MenuItem (bank, i, names[i], accent, i == current));
            menu.addItem (item);
        }

        menu.addSeparator();
        menu.addItem (100001, "Import wavetable (.wav / .json)...");
        menu.addItem (100002, "Rescan wavetable folder");
        menu.addItem (100003, "Show wavetable folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMaximumNumColumns (1).withMinimumWidth (getWidth()),
                            [this, safe = juce::Component::SafePointer<TableSelector> (this)] (int id)
        {
            if (safe == nullptr || id == 0) return;
            if (id == 100001)      importFiles();
            else if (id == 100002) rescan (-1);
            else if (id == 100003) { auto d = ember::WavetableBank::userDirectory(); d.createDirectory(); d.revealToUser(); }
            else                   select (id - 1);
        });
    }

    // rescans the user folder; if `selectSource` is given, picks the table that came from that file
    void rescan (int, const juce::String& selectSource = {})
    {
        auto& bank = proc.getWavetableBank();
        const auto problems = bank.scanUserTables();

        if (selectSource.isNotEmpty())
            for (int i = bank.numFactoryTables(); i < bank.numSlots(); ++i)
                if (bank.info (i).loaded && bank.info (i).source == selectSource)
                    select (i);

        if (problems.size() > 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wavetables",
                                                    "Some wavetable files could not be used:\n\n" + problems.joinIntoString ("\n"));
        repaint();
    }

    void importFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import wavetable(s) - audio (.wav) or a JSON recipe",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, safe = juce::Component::SafePointer<TableSelector> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResults().isEmpty())
                return;

            const auto dir = ember::WavetableBank::userDirectory();
            dir.createDirectory();

            juce::String lastCopied;
            for (auto& f : fc.getResults())
            {
                auto target = dir.getChildFile (f.getFileName());
                for (int n = 2; target.exists() && n < 100; ++n)   // never overwrite a table the user already has
                    target = dir.getChildFile (f.getFileNameWithoutExtension() + " (" + juce::String (n) + ")" + f.getFileExtension());
                if (f.copyFileTo (target))
                    lastCopied = target.getFullPathName();
            }
            rescan (-1, lastCopied);
        });
    }

    EmberlineProcessor& proc;
    juce::Colour accent;
    int current = 0;
    juce::ParameterAttachment attachment;
    std::unique_ptr<juce::FileChooser> chooser;
};
