#pragma once

#include <juce_core/juce_core.h>

// Tempo-sync note values shared by the LFOs and the delay. Index 0 means "not synced".
namespace ember::tempo
{
    inline const juce::StringArray& names()
    {
        static const juce::StringArray n { "Off", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T", "1/4", "1/4.",
                                           "1/2", "1 bar", "2 bars", "4 bars" };
        return n;
    }

    // length of one cycle in quarter-note beats
    inline float beats (int index) noexcept
    {
        static constexpr float table[] { 0.0f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f,
                                         2.0f, 4.0f, 8.0f, 16.0f };
        return table[juce::jlimit (0, (int) std::size (table) - 1, index)];
    }

    inline float rateHz (int syncIndex, float freeHz, double bpm) noexcept
    {
        return syncIndex <= 0 ? freeHz : (float) (bpm / 60.0) / beats (syncIndex);
    }

    inline float timeMs (int syncIndex, float freeMs, double bpm) noexcept
    {
        return syncIndex <= 0 ? freeMs : beats (syncIndex) * (float) (60000.0 / bpm);
    }
}
