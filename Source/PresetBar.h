#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"

// [<] [ preset name v ] [>] [SAVE] - browse factory + user presets, save the current sound as JSON.
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (EmberlineProcessor& p) : proc (p)
    {
        for (auto* b : { &prev, &next, &save })
            addAndMakeVisible (b);
        addAndMakeVisible (nameButton);
        nameButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));

        prev.onClick = [this] { step (-1); };
        next.onClick = [this] { step (+1); };
        nameButton.onClick = [this] { showMenu(); };
        save.onClick = [this] { showSaveDialog(); };

        refreshName();
        startTimerHz (4);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        prev.setBounds (r.removeFromLeft (24));
        r.removeFromLeft (3);
        save.setBounds (r.removeFromRight (48));
        r.removeFromRight (3);
        next.setBounds (r.removeFromRight (24));
        r.removeFromRight (3);
        nameButton.setBounds (r);
    }

private:
    void timerCallback() override { refreshName(); }

    void refreshName()
    {
        const auto name = proc.getCurrentPresetName();
        const auto text = name.isEmpty() ? juce::String ("(custom)") : name;
        if (nameButton.getButtonText() != text)
            nameButton.setButtonText (text);
    }

    int currentIndex() const
    {
        return proc.presetLibrary.indexOf (proc.getCurrentPresetName());
    }

    void step (int dir)
    {
        const int n = (int) proc.presetLibrary.all().size();
        if (n == 0) return;
        const int i = currentIndex();
        proc.loadPreset (i < 0 ? 0 : (i + dir + n) % n);
        refreshName();
    }

    void showMenu()
    {
        const auto& all = proc.presetLibrary.all();
        const int current = currentIndex();

        juce::PopupMenu menu;
        for (const auto& category : proc.presetLibrary.categories())
        {
            juce::PopupMenu sub;
            for (size_t i = 0; i < all.size(); ++i)
                if (all[i].category == category)
                    sub.addItem ((int) i + 1, all[i].name, true, (int) i == current);
            menu.addSubMenu (category == "User" ? "USER" : category, sub);
        }

        menu.addSeparator();
        menu.addItem (100001, "Rescan user presets");
        menu.addItem (100002, "Show user preset folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMaximumNumColumns (1),
                            [this, safe = juce::Component::SafePointer<PresetBar> (this)] (int id)
        {
            if (safe == nullptr || id == 0) return;
            if (id == 100001)      proc.presetLibrary.scanUser();
            else if (id == 100002) { auto d = ember::presets::Library::userDirectory(); d.createDirectory(); d.revealToUser(); }
            else                   proc.loadPreset (id - 1);
            refreshName();
        });
    }

    void showSaveDialog()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Saved as a JSON file in your user preset folder.", juce::MessageBoxIconType::NoIcon, this);
        const auto current = proc.getCurrentPresetName();
        w->addTextEditor ("name", current.isEmpty() ? "My Preset" : current + " (edit)", "Name");

        juce::StringArray cats { "User" };
        for (auto& c : proc.presetLibrary.categories())
            if (c != "User" && c != "Init") cats.add (c);
        w->addComboBox ("category", cats, "Category");
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
        {
            if (safe == nullptr || result != 1) return;
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            const auto* box = w->getComboBoxComponent ("category");
            proc.saveUserPreset (name, box != nullptr ? box->getText() : juce::String ("User"));
            refreshName();
        }), true);
    }

    EmberlineProcessor& proc;
    juce::TextButton prev { "<" }, next { ">" }, save { "SAVE" }, nameButton;
};
