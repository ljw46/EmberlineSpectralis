#pragma once

#include <ManualData.h>
#include <juce_core/juce_core.h>

namespace ember::manual
{
    // The manual is embedded in the plugin as a single HTML page. Opening it writes the page to the user's folder
    // (only if it is missing or from a different build) and hands it to the default browser, so it works offline
    // and always matches the installed version.
    inline juce::File userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Emberline").getChildFile ("Spectralis").getChildFile ("Manual");
    }

    // Writes the embedded page into `dir` and returns the file, or a non-existent File if that failed.
    inline juce::File writeTo (const juce::File& dir)
    {
        if (! dir.createDirectory().wasOk())
            return {};

        const auto file = dir.getChildFile ("Emberline Spectralis Manual.html");
        juce::MemoryBlock existing;
        const bool upToDate = file.existsAsFile() && file.loadFileAsData (existing) && existing.getSize() == (size_t) ManualData::manual_htmlSize
                              && std::memcmp (existing.getData(), ManualData::manual_html, existing.getSize()) == 0;

        if (! upToDate && ! file.replaceWithData (ManualData::manual_html, (size_t) ManualData::manual_htmlSize))
            return {};
        return file;
    }

    inline bool open()
    {
        const auto file = writeTo (userDirectory());
        return file.existsAsFile() && juce::URL (file).launchInDefaultBrowser();
    }
}
