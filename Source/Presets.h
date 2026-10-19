#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class EmberlineProcessor;

// JSON presets. The format is documented in docs/PRESETS.md; in short:
//   { "name", "category", "author", "url", "description", "tags": [...],
//     "parameters": { "<param id>": value, ... },            // real units; choices by name; only non-defaults needed
//     "modulation": [ { "source", "destination", "depth", "curve", "bipolar" }, ... ],
//     "lfo_shapes": { "lfo1": "sine" | [ { "x", "y", "curve" }, ... ] } }
namespace ember::presets
{
    struct Info
    {
        juce::String name, category, description, author, url;
        juce::StringArray tags;
        bool factory = false;
        juce::String json;   // the preset text itself (factory presets are embedded in the binary)
        juce::File file;     // user presets only
    };

    struct Result
    {
        bool ok = true;
        juce::StringArray warnings;   // unknown ids, out-of-range values, ... (the preset still loads)
    };

    class Library
    {
    public:
        void scanFactory();
        void scanUser();

        const std::vector<Info>& all() const noexcept { return items; }
        int numFactory() const noexcept { return factoryCount; }
        int indexOf (const juce::String& name) const;
        juce::StringArray categories() const;

        static juce::File userDirectory();

        // Who the user's own presets are credited to (remembered between saves in a small settings file).
        static void loadAuthor (juce::String& author, juce::String& url);
        static void saveAuthor (const juce::String& author, const juce::String& url);

    private:
        std::vector<Info> items;
        int factoryCount = 0;
    };

    bool parse (const juce::String& text, juce::var& out, juce::String& error);
    juce::String toJson (const juce::var& preset);

    // Message thread only. Resets every parameter to its default first, then applies the preset.
    Result apply (EmberlineProcessor&, const juce::var& preset);

    // Snapshot of the current sound as a preset (only non-default parameters are written).
    juce::var capture (EmberlineProcessor&, const juce::String& name, const juce::String& category,
                       const juce::String& description = {}, const juce::StringArray& tags = {},
                       const juce::String& author = {}, const juce::String& url = {});

    // Only http(s) links are ever opened; a bare "example.com/me" gets https:// added. Empty if unusable.
    juce::String cleanUrl (const juce::String& text);

    juce::String safeFileName (const juce::String& name);
}
