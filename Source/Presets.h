#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class EmberlineProcessor;

// JSON presets. The format is documented in docs/PRESETS.md; in short:
//   { "name", "category", "description", "tags": [...],
//     "parameters": { "<param id>": value, ... },            // real units; choices by name; only non-defaults needed
//     "modulation": [ { "source", "destination", "depth", "curve", "bipolar" }, ... ],
//     "lfo_shapes": { "lfo1": "sine" | [ { "x", "y", "curve" }, ... ] } }
namespace ember::presets
{
    struct Info
    {
        juce::String name, category, description;
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
                       const juce::String& description = {}, const juce::StringArray& tags = {});

    juce::String safeFileName (const juce::String& name);
}
