#include "WavetableBank.h"
#include "Expression.h"
#include <WavetableData.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <memory>

namespace ember
{
    namespace
    {
        constexpr int N = WavetableBank::tableSize;
        constexpr int H = WavetableBank::maxHarmonics;
        constexpr int F = WavetableBank::numFrames;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        // A frame as harmonic coefficients: sum over h of s[h] sin(h t) + c[h] cos(h t)
        struct Spectrum
        {
            std::array<float, H + 1> s {}, c {};
        };

        // FFT <-> (sin, cos) coefficients. The scale factors are *calibrated* against known sines and
        // cosines at construction, so no assumption about the FFT library's conventions is baked in.
        class Transform
        {
        public:
            Transform() : fft (11)
            {
                std::array<float, N> sine {}, cosine {};
                for (int n = 0; n < N; ++n)
                {
                    sine[(size_t) n]   = std::sin (twoPi * 5.0f * (float) n / (float) N);
                    cosine[(size_t) n] = std::cos (twoPi * 5.0f * (float) n / (float) N);
                }

                auto a = std::make_unique<Spectrum>(), b = std::make_unique<Spectrum>(), unit = std::make_unique<Spectrum>();
                toSpectrumRaw (sine.data(), *a);
                toSpectrumRaw (cosine.data(), *b);
                analysisSin = 1.0f / a->s[5];
                analysisCos = 1.0f / b->c[5];

                std::array<float, N> out {};
                unit->s[5] = 1.0f;
                toTimeRaw (*unit, 5, out.data());
                synthSin = 1.0f / dot (out.data(), sine.data());
                unit->s[5] = 0.0f;
                unit->c[5] = 1.0f;
                toTimeRaw (*unit, 5, out.data());
                synthCos = 1.0f / dot (out.data(), cosine.data());
            }

            void toSpectrum (const float* x, Spectrum& out)
            {
                toSpectrumRaw (x, out);
                for (int h = 1; h <= H; ++h)
                {
                    out.s[(size_t) h] *= analysisSin;
                    out.c[(size_t) h] *= analysisCos;
                }
            }

            void toTime (const Spectrum& sp, int hMax, float* out)
            {
                // Band-limiting is a plain truncation: it keeps every harmonic up to the mip's limit at full level.
                // (A brick wall rings at the very top of the band - Gibbs - which costs ~1.4 dB of peak headroom on a
                // square or saw; tapering it away needs a full-width window that audibly dulls the tables, so we don't.)
                scaled = {};
                for (int h = 1; h <= hMax; ++h)
                {
                    scaled.s[(size_t) h] = sp.s[(size_t) h] * synthSin;
                    scaled.c[(size_t) h] = sp.c[(size_t) h] * synthCos;
                }
                toTimeRaw (scaled, hMax, out);
            }

        private:
            static float dot (const float* a, const float* b)
            {
                double s = 0.0, n = 0.0;
                for (int i = 0; i < N; ++i) { s += (double) a[i] * b[i]; n += (double) b[i] * b[i]; }
                return (float) (s / n);
            }

            void toSpectrumRaw (const float* x, Spectrum& out)
            {
                std::array<float, 2 * N> d {};
                std::copy (x, x + N, d.begin());
                fft.performRealOnlyForwardTransform (d.data());
                for (int h = 1; h <= H; ++h)
                {
                    out.c[(size_t) h] = d[(size_t) (2 * h)];
                    out.s[(size_t) h] = -d[(size_t) (2 * h + 1)];
                }
            }

            void toTimeRaw (const Spectrum& sp, int hMax, float* out)
            {
                std::array<float, 2 * N> d {};
                for (int h = 1; h <= hMax; ++h)
                {
                    const float re = sp.c[(size_t) h], im = -sp.s[(size_t) h];
                    d[(size_t) (2 * h)] = re;
                    d[(size_t) (2 * h + 1)] = im;
                    d[(size_t) (2 * (N - h))] = re;
                    d[(size_t) (2 * (N - h) + 1)] = -im;
                }
                fft.performRealOnlyInverseTransform (d.data());
                std::copy (d.begin(), d.begin() + N, out);
            }

            juce::dsp::FFT fft;
            Spectrum scaled;
            float analysisSin = 1, analysisCos = 1, synthSin = 1, synthCos = 1;
        };

        float lerp (float a, float b, float t) { return a + (b - a) * t; }

        using Frames = std::vector<std::unique_ptr<Spectrum>>;

        Frames makeFrames()
        {
            Frames f;
            for (int i = 0; i < F; ++i) f.push_back (std::make_unique<Spectrum>());
            return f;
        }

        // 16 frames of harmonic spectra -> mip-mapped, normalised table data
        bool synthesise (Transform& transform, const Frames& frames, std::vector<float>& data, juce::String& error)
        {
            data.assign (WavetableBank::blockSize, 0.0f);
            bool anySound = false;

            for (int frame = 0; frame < F; ++frame)
            {
                const auto& sp = *frames[(size_t) frame];
                float norm = 1.0f;   // normalise to the full-bandwidth peak so every mip of a frame shares one gain

                for (int mip = 0; mip < WavetableBank::numMips; ++mip)
                {
                    auto* out = data.data() + ((size_t) mip * F + (size_t) frame) * N;
                    transform.toTime (sp, WavetableBank::harmonicsForMip (mip), out);

                    if (mip == 0)
                    {
                        float peak = 0.0f;
                        for (int n = 0; n < N; ++n)
                        {
                            if (! std::isfinite (out[n])) { error = "frame " + juce::String (frame + 1) + " produced a non-finite value"; return false; }
                            peak = juce::jmax (peak, std::abs (out[n]));
                        }
                        if (peak > 1.0e-5f) { norm = 1.0f / peak; anySound = true; }
                        else norm = 0.0f;
                    }

                    for (int n = 0; n < N; ++n)
                        out[n] *= norm;
                }
            }

            if (! anySound) { error = "the table is silent (every frame is empty)"; return false; }
            return true;
        }

        void readHarmonicEntry (const juce::var& v, int h, Spectrum& sp)
        {
            if (h < 1 || h > H) return;
            double amp = 0.0, phase = 0.0;
            if (auto* arr = v.getArray())
            {
                if (arr->size() >= 1) amp = (double) (*arr)[0];
                if (arr->size() >= 2) phase = (double) (*arr)[1];
            }
            else
                amp = (double) v;

            const double p = juce::MathConstants<double>::twoPi * phase;   // amp * sin(h t + p)
            sp.s[(size_t) h] += (float) (amp * std::cos (p));
            sp.c[(size_t) h] += (float) (amp * std::sin (p));
        }

        bool readStage (const juce::var& stage, int limit, Spectrum& sp, juce::String& error)
        {
            if (stage.hasProperty ("harmonics"))
            {
                const auto hv = stage["harmonics"];
                if (auto* arr = hv.getArray())
                    for (int i = 0; i < arr->size(); ++i)
                        readHarmonicEntry ((*arr)[i], i + 1, sp);
                else if (auto* obj = hv.getDynamicObject())
                    for (auto& nv : obj->getProperties())
                        readHarmonicEntry (nv.value, nv.name.toString().getIntValue(), sp);
                else
                {
                    error = "\"harmonics\" must be a list or an object";
                    return false;
                }
                return true;
            }

            if (! stage.hasProperty ("amplitude"))
            {
                error = "a stage needs \"amplitude\" (an expression in h) or \"harmonics\"";
                return false;
            }

            juce::String e;
            auto amp = Expression::compile (stage["amplitude"].toString(), { "h" }, e);
            if (! amp) { error = "amplitude: " + e; return false; }
            std::unique_ptr<Expression> cosine;
            if (stage.hasProperty ("cosine"))
            {
                cosine = Expression::compile (stage["cosine"].toString(), { "h" }, e);
                if (! cosine) { error = "cosine: " + e; return false; }
            }

            for (int h = 1; h <= limit; ++h)
            {
                const double v[1] { (double) h };
                sp.s[(size_t) h] = (float) amp->evaluate (v);
                if (cosine) sp.c[(size_t) h] = (float) cosine->evaluate (v);
            }
            return true;
        }
    }

    // ------------------------------------------------------------------------------------ building
    WavetableBank::Built WavetableBank::buildFromJson (const juce::var& def, const FileResolver& resolver)
    {
        Built b;
        auto fail = [&b] (const juce::String& m) { b.error = m; return std::move (b); };

        if (def.getDynamicObject() == nullptr)
            return fail ("a wavetable must be a JSON object");

        b.info.name = def["name"].toString().trim();
        if (b.info.name.isEmpty())
            return fail ("missing \"name\"");
        b.info.category = def.getProperty ("category", "User").toString();
        b.info.description = def["description"].toString();
        if (auto* tags = def["tags"].getArray())
            for (auto& t : *tags) b.info.tags.add (t.toString());

        const auto mode = def["mode"].toString().toLowerCase();

        if (mode == "wav")   // the recipe is a recording: "file" names it (embedded in the plugin, or next to the JSON for user tables)
        {
            const auto file = def["file"].toString();
            if (file.isEmpty())
                return fail ("mode \"wav\" needs \"file\" (the name of the .wav)");
            juce::MemoryBlock bytes;
            if (! resolver || ! resolver (file, bytes))
                return fail ("could not find the audio file \"" + file + "\"");

            auto audio = buildFromAudio (std::make_unique<juce::MemoryInputStream> (bytes, true), b.info.name, file,
                                         (int) def.getProperty ("frame_size", 0), (bool) def.getProperty ("align_phase", true));
            if (! audio.ok)
                return fail (audio.error);

            audio.info.category = b.info.category;
            audio.info.description = b.info.description.isNotEmpty() ? b.info.description : audio.info.description;
            audio.info.tags = b.info.tags;
            audio.info.source = file;
            return audio;
        }

        const int limit = juce::jlimit (1, H, (int) def.getProperty ("harmonic_limit", H));
        auto frames = makeFrames();
        Transform transform;
        juce::String e;

        if (mode == "spectrum")
        {
            auto amp = Expression::compile (def["amplitude"].toString(), { "h", "t", "frame" }, e);
            if (! amp) return fail ("amplitude: " + e);
            std::unique_ptr<Expression> cosine;
            if (def.hasProperty ("cosine"))
            {
                cosine = Expression::compile (def["cosine"].toString(), { "h", "t", "frame" }, e);
                if (! cosine) return fail ("cosine: " + e);
            }

            for (int f = 0; f < F; ++f)
                for (int h = 1; h <= limit; ++h)
                {
                    const double v[3] { (double) h, (double) f / (F - 1), (double) f };
                    frames[(size_t) f]->s[(size_t) h] = (float) amp->evaluate (v);
                    if (cosine) frames[(size_t) f]->c[(size_t) h] = (float) cosine->evaluate (v);
                }
        }
        else if (mode == "waveform")
        {
            auto expr = Expression::compile (def["expression"].toString(), { "phase", "t", "frame" }, e);
            if (! expr) return fail ("expression: " + e);

            std::array<float, N> x {};
            for (int f = 0; f < F; ++f)
            {
                for (int n = 0; n < N; ++n)
                {
                    const double v[3] { (double) n / N, (double) f / (F - 1), (double) f };
                    x[(size_t) n] = (float) expr->evaluate (v);
                }
                transform.toSpectrum (x.data(), *frames[(size_t) f]);
                for (int h = limit + 1; h <= H; ++h)   // honour harmonic_limit here too
                    frames[(size_t) f]->s[(size_t) h] = frames[(size_t) f]->c[(size_t) h] = 0.0f;
            }
        }
        else if (mode == "morph")
        {
            auto* stages = def["stages"].getArray();
            if (stages == nullptr || stages->size() < 2)
                return fail ("\"stages\" must be a list of at least two stages");
            if (stages->size() > 32)
                return fail ("at most 32 stages");

            std::vector<std::unique_ptr<Spectrum>> built;
            for (int i = 0; i < stages->size(); ++i)
            {
                auto sp = std::make_unique<Spectrum>();
                if (! readStage ((*stages)[i], limit, *sp, e))
                    return fail ("stage " + juce::String (i + 1) + ": " + e);
                built.push_back (std::move (sp));
            }

            for (int f = 0; f < F; ++f)
            {
                const float pos = (float) f / (F - 1) * (float) (built.size() - 1);
                const int a = juce::jmin ((int) pos, (int) built.size() - 2);
                const float fr = pos - (float) a;
                for (int h = 1; h <= H; ++h)
                {
                    frames[(size_t) f]->s[(size_t) h] = lerp (built[(size_t) a]->s[(size_t) h], built[(size_t) a + 1]->s[(size_t) h], fr);
                    frames[(size_t) f]->c[(size_t) h] = lerp (built[(size_t) a]->c[(size_t) h], built[(size_t) a + 1]->c[(size_t) h], fr);
                }
            }
        }
        else
            return fail ("\"mode\" must be \"spectrum\", \"waveform\", \"morph\" or \"wav\"");

        if (! synthesise (transform, frames, b.data, e))
            return fail (e);

        b.info.loaded = true;
        b.ok = true;
        return b;
    }

    // Audio becomes a wavetable. A file whose length is a multiple of the frame size (2048 unless told otherwise) is
    // read as a multi-frame table (the Serum / Vital layout); anything else is a single cycle of any length.
    // Frames are phase-aligned and then interpolated *spectrally* down (or up) to the 16 frames, so morphing between
    // recorded frames does not cancel and comb-filter the way a plain waveform cross-fade would.
    WavetableBank::Built WavetableBank::buildFromAudio (std::unique_ptr<juce::InputStream> stream, const juce::String& name,
                                                        const juce::String& source, int frameSize, bool alignPhase)
    {
        Built b;
        b.info.name = name;
        b.info.category = "User";
        b.info.description = "Imported from " + source;
        b.info.source = source;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (std::move (stream)));
        if (reader == nullptr || reader->lengthInSamples < 64)
        {
            b.error = "could not read audio (or it is too short)";
            return b;
        }

        const int length = (int) juce::jmin ((juce::int64) 4 * 1000 * 1000, reader->lengthInSamples);
        juce::AudioBuffer<float> audio ((int) reader->numChannels, length);
        reader->read (&audio, 0, length, 0, true, true);

        std::vector<float> mono ((size_t) length, 0.0f);
        for (int c = 0; c < audio.getNumChannels(); ++c)
            for (int i = 0; i < length; ++i)
                mono[(size_t) i] += audio.getSample (c, i) / (float) audio.getNumChannels();

        int frameLen;
        int sourceFrames;
        if (frameSize >= 32)
        {
            frameLen = frameSize;
            sourceFrames = juce::jmax (1, length / frameSize);
        }
        else if (length >= 2 * N && length % N == 0)
        {
            frameLen = N;
            sourceFrames = length / N;
        }
        else
        {
            frameLen = length;
            sourceFrames = 1;
        }

        // one source cycle -> N samples (cubic interpolation, wrapping)
        auto resampleFrame = [&] (int frame, std::array<float, N>& out)
        {
            const size_t base = (size_t) frame * (size_t) frameLen;
            auto at = [&] (int i) { return mono[base + (size_t) (((i % frameLen) + frameLen) % frameLen)]; };
            for (int n = 0; n < N; ++n)
            {
                const float pos = (float) n * (float) frameLen / (float) N;
                const int i = (int) pos;
                const float f = pos - (float) i;
                const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
                const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                out[(size_t) n] = ((c3 * f + c2) * f + c1) * f + y1;
            }
        };

        Transform transform;
        std::vector<std::unique_ptr<Spectrum>> source_spectra;
        std::array<float, N> x {};
        const int sourceLimit = frameLen < N ? juce::jmax (4, frameLen / 2 - 1) : H;   // a short cycle has nothing above its own Nyquist

        for (int f = 0; f < sourceFrames; ++f)
        {
            resampleFrame (f, x);
            auto sp = std::make_unique<Spectrum>();
            transform.toSpectrum (x.data(), *sp);
            for (int h = sourceLimit + 1; h <= H; ++h)
                sp->s[(size_t) h] = sp->c[(size_t) h] = 0.0f;

            if (alignPhase)
            {
                // choose the harmonic that defines the frame's phase: the fundamental unless it is weak
                float maxAmp = 0.0f;
                int dominant = 1;
                for (int h = 1; h <= 8; ++h)
                {
                    const float amp = std::hypot (sp->s[(size_t) h], sp->c[(size_t) h]);
                    if (amp > maxAmp) { maxAmp = amp; dominant = h; }
                }
                if (std::hypot (sp->s[1], sp->c[1]) >= 0.1f * maxAmp)
                    dominant = 1;

                if (maxAmp > 1.0e-6f)
                {
                    // amp * sin(h t + phi): s = amp cos(phi), c = amp sin(phi); shift so the dominant harmonic has phi = 0
                    const float phi = std::atan2 (sp->c[(size_t) dominant], sp->s[(size_t) dominant]);
                    const float shift = -phi / (float) dominant;
                    for (int h = 1; h <= sourceLimit; ++h)
                    {
                        const float amp = std::hypot (sp->s[(size_t) h], sp->c[(size_t) h]);
                        const float ph = std::atan2 (sp->c[(size_t) h], sp->s[(size_t) h]) + (float) h * shift;
                        sp->s[(size_t) h] = amp * std::cos (ph);
                        sp->c[(size_t) h] = amp * std::sin (ph);
                    }
                }
            }
            source_spectra.push_back (std::move (sp));
        }

        auto frames = makeFrames();
        for (int f = 0; f < F; ++f)
        {
            const float pos = sourceFrames > 1 ? (float) f / (F - 1) * (float) (sourceFrames - 1) : 0.0f;
            const int a = juce::jmin ((int) pos, juce::jmax (0, sourceFrames - 2));
            const float fr = sourceFrames > 1 ? pos - (float) a : 0.0f;
            const auto& s0 = *source_spectra[(size_t) a];
            const auto& s1 = *source_spectra[(size_t) juce::jmin (a + 1, sourceFrames - 1)];
            for (int h = 1; h <= H; ++h)
            {
                frames[(size_t) f]->s[(size_t) h] = lerp (s0.s[(size_t) h], s1.s[(size_t) h], fr);
                frames[(size_t) f]->c[(size_t) h] = lerp (s0.c[(size_t) h], s1.c[(size_t) h], fr);
            }
        }

        if (! synthesise (transform, frames, b.data, b.error))
            return b;

        b.info.description += sourceFrames > 1 ? " (" + juce::String (sourceFrames) + " frames, resampled to 16)" : " (single cycle)";
        b.info.loaded = true;
        b.ok = true;
        return b;
    }

    WavetableBank::Built WavetableBank::buildFromWav (const juce::File& file)
    {
        auto built = buildFromAudio (std::unique_ptr<juce::InputStream> (file.createInputStream()), file.getFileNameWithoutExtension(), file.getFileName(), 0, true);
        built.info.source = file.getFullPathName();
        return built;
    }

    // ------------------------------------------------------------------------------------ the bank
    juce::File WavetableBank::userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Emberline").getChildFile ("Spectralis").getChildFile ("Wavetables");
    }

    WavetableBank& WavetableBank::shared()
    {
        static WavetableBank bank;   // thread-safe, built on first use
        return bank;
    }

    WavetableBank::WavetableBank()
    {
        struct Item { int id; Built built; };
        std::vector<Item> items;

        // WAV-backed tables find their audio among the embedded resources, by file name
        const FileResolver embedded = [] (const juce::String& name, juce::MemoryBlock& out)
        {
            const auto wanted = juce::File::createFileWithoutCheckingPath (name).getFileName();
            for (int i = 0; i < WavetableData::namedResourceListSize; ++i)
                if (juce::String (WavetableData::getNamedResourceOriginalFilename (WavetableData::namedResourceList[i])) == wanted)
                {
                    int size = 0;
                    const char* bytes = WavetableData::getNamedResource (WavetableData::namedResourceList[i], size);
                    out.replaceAll (bytes, (size_t) size);
                    return true;
                }
            return false;
        };

        for (int i = 0; i < WavetableData::namedResourceListSize; ++i)
        {
            if (! juce::String (WavetableData::getNamedResourceOriginalFilename (WavetableData::namedResourceList[i])).endsWithIgnoreCase (".json"))
                continue;   // audio files are pulled in by the tables that name them

            int size = 0;
            const char* text = WavetableData::getNamedResource (WavetableData::namedResourceList[i], size);

            juce::var v;
            if (juce::JSON::parse (juce::String::fromUTF8 (text, size), v).failed())
            {
                DBG ("Bad factory wavetable JSON: " << WavetableData::namedResourceList[i]);
                continue;
            }

            auto built = buildFromJson (v, embedded);
            if (! built.ok)
            {
                DBG ("Bad factory wavetable " << WavetableData::namedResourceList[i] << ": " << built.error);
                continue;
            }
            built.info.factory = true;
            items.push_back ({ (int) v.getProperty ("id", 1000000 + i), std::move (built) });
        }

        std::stable_sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.id < b.id; });

        factoryCount = (int) items.size();
        data.assign ((size_t) (factoryCount + numUserSlots) * blockSize, 0.0f);   // fixed size: never reallocated afterwards
        infos.resize ((size_t) (factoryCount + numUserSlots));

        for (int i = 0; i < factoryCount; ++i)
            install (i, items[(size_t) i].built);

        for (int i = 0; i < numUserSlots; ++i)
            infos[(size_t) (factoryCount + i)].name = "User " + juce::String (i + 1);

        scanUserTables();
    }

    void WavetableBank::install (int slot, const Built& built)
    {
        std::copy (built.data.begin(), built.data.end(), data.begin() + (std::ptrdiff_t) ((size_t) slot * blockSize));
        infos[(size_t) slot] = built.info;
    }

    juce::StringArray WavetableBank::paramChoiceNames() const
    {
        juce::StringArray n;
        for (int i = 0; i < factoryCount; ++i) n.add (infos[(size_t) i].name);
        for (int i = 0; i < numUserSlots; ++i) n.add ("User " + juce::String (i + 1));
        return n;
    }

    juce::StringArray WavetableBank::displayNames() const
    {
        juce::StringArray n;
        for (int i = 0; i < numSlots(); ++i)
        {
            const auto& t = infos[(size_t) i];
            n.add (i < factoryCount ? t.name : (t.loaded ? t.name : "User " + juce::String (i - factoryCount + 1) + " (empty)"));
        }
        return n;
    }

    juce::StringArray WavetableBank::displayCategories() const
    {
        juce::StringArray c;
        for (int i = 0; i < numSlots(); ++i)
            c.add (i < factoryCount ? infos[(size_t) i].category : juce::String ("User"));
        return c;
    }

    int WavetableBank::indexOfName (const juce::String& name) const
    {
        for (int i = 0; i < numSlots(); ++i)
            if ((i < factoryCount || infos[(size_t) i].loaded) && infos[(size_t) i].name.equalsIgnoreCase (name))
                return i;
        for (int i = 0; i < numUserSlots; ++i)   // "User 3" always addresses the third slot
            if (name.equalsIgnoreCase ("User " + juce::String (i + 1)))
                return factoryCount + i;
        return -1;
    }

    juce::StringArray WavetableBank::scanUserTables()
    {
        juce::StringArray problems;
        auto files = userDirectory().findChildFiles (juce::File::findFiles, false, "*.json;*.wav");
        files.sort();

        for (int slot = 0; slot < numUserSlots; ++slot)
        {
            const int index = factoryCount + slot;
            infos[(size_t) index] = {};
            infos[(size_t) index].name = "User " + juce::String (slot + 1);
            std::fill (data.begin() + (std::ptrdiff_t) ((size_t) index * blockSize),
                       data.begin() + (std::ptrdiff_t) ((size_t) (index + 1) * blockSize), 0.0f);

            if (slot >= files.size())
                continue;

            const auto& file = files.getReference (slot);
            Built built;
            if (file.hasFileExtension ("wav"))
                built = buildFromWav (file);
            else
            {
                juce::var v;
                const auto parsed = juce::JSON::parse (file.loadFileAsString(), v);
                if (parsed.failed()) built.error = "invalid JSON: " + parsed.getErrorMessage();
                else
                {
                    const auto folder = file.getParentDirectory();
                    built = buildFromJson (v, [folder] (const juce::String& name, juce::MemoryBlock& out) { return folder.getChildFile (name).loadFileAsData (out); });
                }
                built.info.source = file.getFullPathName();
            }

            if (built.ok)
                install (index, built);
            else
                problems.add (file.getFileName() + ": " + built.error);
        }

        if (files.size() > numUserSlots)
            problems.add ("only the first " + juce::String (numUserSlots) + " files are used (" + juce::String (files.size()) + " found)");
        return problems;
    }
}
