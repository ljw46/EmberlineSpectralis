#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace ember
{
    // The wavetable library. Every table has 16 frames, and every frame is band-limited at 10 mip levels so
    // the oscillator can pick the level whose top harmonic stays below Nyquist for the note being played.
    //
    // Tables are plain JSON files (see docs/WAVETABLES.md): the factory set in Wavetables/Factory is embedded
    // into the plugin, and users / LLMs can drop more .json (or .wav) files into the user wavetable folder -
    // they fill the "User 1".."User 8" slots. The shared instance is built once per process.
    // Audio threads only ever call sample()/table(); everything else is message-thread only.
    class WavetableBank
    {
    public:
        static constexpr int tableSize = 2048;
        static constexpr int numFrames = 16;
        static constexpr int numMips   = 10;
        static constexpr int maxHarmonics = tableSize / 2 - 1;   // 1023 at mip 0, halved per level
        static constexpr int numUserSlots = 8;
        static constexpr size_t blockSize = (size_t) numMips * numFrames * tableSize;   // floats per table

        struct TableInfo
        {
            juce::String name, category, description, source;
            juce::StringArray tags;
            bool factory = false, loaded = false;
        };

        // The result of compiling one definition (JSON text or a .wav file); `data` is blockSize floats.
        struct Built
        {
            bool ok = false;
            juce::String error;
            TableInfo info;
            std::vector<float> data;
        };

        static WavetableBank& shared();

        static Built buildFromJson (const juce::var& definition);
        static Built buildFromWav (const juce::File& file);
        static juce::File userDirectory();

        int numFactoryTables() const noexcept { return factoryCount; }
        int numSlots() const noexcept         { return factoryCount + numUserSlots; }
        const TableInfo& info (int index) const { return infos[(size_t) index]; }

        juce::StringArray paramChoiceNames() const;   // factory names, then "User 1".."User 8" (fixed: baked into the parameter)
        juce::StringArray displayNames() const;       // user slots show the name of the table loaded into them
        juce::StringArray displayCategories() const;   // factory category per table; "User" for the user slots
        int indexOfName (const juce::String& name) const;   // factory or user table by name, -1 if unknown

        juce::StringArray scanUserTables();           // (re)loads the user folder; returns problems, one line each

        static constexpr int harmonicsForMip (int mip) { return maxHarmonics >> mip; }

        // Smallest (brightest) mip whose highest harmonic fits under `allowedHarmonics`.
        static int mipFor (float allowedHarmonics) noexcept
        {
            int m = 0;
            while (m < numMips - 1 && (float) harmonicsForMip (m) > allowedHarmonics)
                ++m;
            return m;
        }

        // frame in [0, numFrames-1], phase in [0, 1). Cubic Hermite along phase, linear across frames.
        float sample (int tableIndex, int mip, float frame, float phase) const noexcept
        {
            int f0 = (int) frame;
            if (f0 > numFrames - 2) f0 = numFrames - 2;
            const float fr = frame - (float) f0;

            const float pos = phase * (float) tableSize;
            const int i = (int) pos;
            const float f = pos - (float) i;

            const auto a = hermite (table (tableIndex, mip, f0),     i, f);
            const auto b = hermite (table (tableIndex, mip, f0 + 1), i, f);
            return a + (b - a) * fr;
        }

        // raw access for displays / tests (mip 0 = full bandwidth)
        const float* table (int tableIndex, int mip, int frame) const noexcept
        {
            return data.data() + ((size_t) tableIndex * blockSize) + ((size_t) mip * numFrames + (size_t) frame) * tableSize;
        }

    private:
        WavetableBank();

        static constexpr int mask = tableSize - 1;

        static float hermite (const float* t, int i, float f) noexcept
        {
            const float y0 = t[(i - 1) & mask], y1 = t[i & mask],
                        y2 = t[(i + 1) & mask], y3 = t[(i + 2) & mask];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

        void install (int slot, const Built& built);

        int factoryCount = 0;
        std::vector<float> data;          // allocated once (never resized), so audio threads can read it safely
        std::vector<TableInfo> infos;
    };
}
