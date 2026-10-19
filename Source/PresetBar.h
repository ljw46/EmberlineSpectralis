#pragma once

#include "EmberLookAndFeel.h"
#include "PluginProcessor.h"
#include "Links.h"
#include "WavetableBank.h"

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
        menu.addItem (100003, "Preset info...", current >= 0);
        menu.addItem (100004, "Export current sound as .json...");
        menu.addItem (100005, "Import preset file(s)...");
        menu.addSeparator();
        menu.addItem (100001, "Rescan user presets");
        menu.addItem (100002, "Show user preset folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMaximumNumColumns (1),
                            [this, safe = juce::Component::SafePointer<PresetBar> (this)] (int id)
        {
            if (safe == nullptr || id == 0) return;
            if (id == 100001)      proc.presetLibrary.scanUser();
            else if (id == 100003) showInfo();
            else if (id == 100004) exportCurrent();
            else if (id == 100005) importFiles();
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

        juce::String author, url;
        ember::presets::Library::loadAuthor (author, url);
        const int cur = currentIndex();
        const auto* info = cur >= 0 ? &proc.presetLibrary.all()[(size_t) cur] : nullptr;
        w->addTextEditor ("description", info != nullptr && ! info->factory ? info->description : juce::String(), "Description");
        w->addTextEditor ("tags", info != nullptr && ! info->factory ? info->tags.joinIntoString (", ") : juce::String(), "Tags (comma separated)");
        w->addTextEditor ("author", author, "Author / artist name");
        w->addTextEditor ("url", url, "Link (website or social, optional)");
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
        {
            if (safe == nullptr || result != 1) return;
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            const auto* box = w->getComboBoxComponent ("category");
            auto tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
            tags.trim();
            tags.removeEmptyStrings();
            const auto who = w->getTextEditorContents ("author").trim();
            const auto link = ember::presets::cleanUrl (w->getTextEditorContents ("url"));
            ember::presets::Library::saveAuthor (who, link);   // remembered for next time
            proc.saveUserPreset (name, box != nullptr ? box->getText() : juce::String ("User"),
                                 w->getTextEditorContents ("description").trim(), tags, who, link);
            refreshName();
        }), true);
    }

    void showInfo()
    {
        const int i = currentIndex();
        if (i < 0) return;
        const auto info = proc.presetLibrary.all()[(size_t) i];
        juce::String text = info.category + (info.factory ? "  |  factory" : "  |  user");
        text << "\nBy: " << (info.author.isEmpty() ? juce::String ("(not credited)") : info.author);
        if (info.url.isNotEmpty()) text << "\n" << info.url;
        if (info.description.isNotEmpty()) text << "\n\n" << info.description;
        if (! info.tags.isEmpty()) text << "\n\nTags: " << info.tags.joinIntoString (", ");

        auto* w = new juce::AlertWindow (info.name, text, juce::MessageBoxIconType::NoIcon, this);
        if (info.url.isNotEmpty()) w->addButton ("Open link", 2);
        w->addButton ("Close", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([url = info.url] (int result)
        {
            if (result == 2) juce::URL (url).launchInDefaultBrowser();
        }), true);
    }

    // Writes the current sound to a .json of the user's choosing, keeping the loaded preset's credits and notes.
    void exportCurrent()
    {
        juce::String author, url, description, category = "User";
        juce::StringArray tags;
        ember::presets::Library::loadAuthor (author, url);
        const int i = currentIndex();
        const auto name = proc.getCurrentPresetName().isEmpty() ? juce::String ("My Preset") : proc.getCurrentPresetName();
        if (i >= 0)
        {
            const auto& info = proc.presetLibrary.all()[(size_t) i];
            description = info.description;
            tags = info.tags;
            category = info.category;
            if (info.author.isNotEmpty()) { author = info.author; url = info.url; }
        }

        const auto preset = ember::presets::capture (proc, name, category, description, tags, author, url);

        // user wavetables are not inside the preset, so whoever receives it needs those files too
        juce::StringArray custom;
        auto& bank = ember::WavetableBank::shared();
        if (auto* params = preset.getProperty ("parameters", {}).getDynamicObject())
            for (auto& prop : params->getProperties())
                if (prop.name.toString().endsWith ("_table"))
                {
                    const auto table = prop.value.toString();
                    bool factory = false;
                    for (int t = 0; t < bank.numFactoryTables(); ++t)
                        factory = factory || bank.info (t).name == table;
                    if (! factory) custom.addIfNotAlreadyThere (table);
                }
        const auto text = ember::presets::toJson (preset);

        chooser = std::make_unique<juce::FileChooser> ("Export preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                                             .getChildFile (ember::presets::safeFileName (name) + ".json"), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, text, custom, safe = juce::Component::SafePointer<PresetBar> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            const auto file = fc.getResult().withFileExtension ("json");
            const bool ok = file.replaceWithText (text, false, false, "\n");
            juce::String msg = ok ? "Saved " + file.getFullPathName() : "Could not write " + file.getFullPathName();
            if (ok && ! custom.isEmpty())
                msg << "\n\nThis preset uses wavetables that are not factory tables (" << custom.joinIntoString (", ")
                    << "). Send those files along too, or it will fall back to a different table.";
            juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "Export preset", msg);
        });
    }

    void importFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import preset file(s)", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, safe = juce::Component::SafePointer<PresetBar> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            int added = 0;
            juce::StringArray skipped;
            const auto dir = ember::presets::Library::userDirectory();
            dir.createDirectory();
            for (auto& f : fc.getResults())
            {
                juce::var v;
                juce::String error;
                if (! ember::presets::parse (f.loadFileAsString(), v, error) || v.getProperty ("name", {}).toString().isEmpty()
                    || v.getProperty ("parameters", {}).getDynamicObject() == nullptr)
                {
                    skipped.add (f.getFileName());
                    continue;
                }
                auto target = dir.getChildFile (ember::presets::safeFileName (v.getProperty ("name", {}).toString()) + ".json");
                for (int n = 2; target.existsAsFile(); ++n)   // never overwrite an existing preset
                    target = dir.getChildFile (ember::presets::safeFileName (v.getProperty ("name", {}).toString()) + " (" + juce::String (n) + ").json");
                if (f.copyFileTo (target)) ++added; else skipped.add (f.getFileName());
            }
            proc.presetLibrary.scanUser();
            refreshName();
            if (added > 0 || ! skipped.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (skipped.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "Import presets",
                                                        "Imported " + juce::String (added) + " preset(s) into your user preset folder."
                                                        + (skipped.isEmpty() ? juce::String() : "\nSkipped (not a valid preset): " + skipped.joinIntoString (", ")));
        });
    }

    std::unique_ptr<juce::FileChooser> chooser;
    EmberlineProcessor& proc;
    juce::TextButton prev { "<" }, next { ">" }, save { "SAVE" }, nameButton;
};

// "by <artist>" plus the link underneath, top right of the header. Clicking opens the preset author's link.
class PresetCredit final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PresetCredit (EmberlineProcessor& p) : proc (p)
    {
        refresh();
        startTimerHz (4);
    }

    void paint (juce::Graphics& g) override
    {
        if (author.isEmpty()) return;
        auto r = getLocalBounds().toFloat();
        const bool link = url.isNotEmpty();
        const auto hot = link && hover;

        g.setColour ((hot ? ember::colours::cyan.brighter (0.3f) : ember::colours::cyan).withAlpha (0.95f));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText ("by " + author, r.removeFromTop (18.0f), juce::Justification::centredRight);

        if (link)
        {
            g.setColour (ember::colours::coral.withAlpha (hot ? 1.0f : 0.8f));
            g.setFont (juce::FontOptions (11.5f));
            g.drawText (shortUrl, r, juce::Justification::centredRight);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (url.isNotEmpty() && e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
            juce::URL (url).launchInDefaultBrowser();
    }

private:
    void timerCallback() override { refresh(); }

    void refresh()
    {
        const int i = proc.presetLibrary.indexOf (proc.getCurrentPresetName());
        juce::String a, u;
        if (i >= 0)
        {
            const auto& info = proc.presetLibrary.all()[(size_t) i];
            a = info.author;
            u = info.url;
        }
        if (a == author && u == url) return;
        author = a;
        url = u;
        shortUrl = u.fromFirstOccurrenceOf ("://", false, false).replace ("www.", "").trimCharactersAtEnd ("/");
        setMouseCursor (url.isNotEmpty() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        setInterceptsMouseClicks (url.isNotEmpty(), false);
        setTooltip (url.isNotEmpty() ? "Open " + url : juce::String());
        repaint();
    }

    EmberlineProcessor& proc;
    juce::String author, url, shortUrl;
    bool hover = false;
};

// A small, dim "buy me a coffee" line under the credit. Never a popup, never in the way.
class SupportLink final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SupportLink()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Enjoying Emberline Spectralis? Buy EmberlineSky a coffee");
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour ((hover ? ember::colours::cyan : juce::Colours::white).withAlpha (hover ? 0.95f : 0.38f));
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("buy me a coffee", getLocalBounds(), juce::Justification::centredRight);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
            juce::URL (ember::links::support).launchInDefaultBrowser();
    }

private:
    bool hover = false;
};
