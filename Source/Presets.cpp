#include "Presets.h"
#include "PluginProcessor.h"
#include "Version.h"
#include "WavetableBank.h"
#include <PresetData.h>

namespace ember::presets
{
    namespace
    {
        // display order of the categories; anything else follows alphabetically
        const juce::StringArray categoryOrder { "Init", "Bass", "Pluck", "Lead", "Pad", "Keys", "Chord", "Arp", "Organic", "Texture", "FX", "User" };

        bool readNumber (const juce::var& v, double& out)
        {
            if (v.isInt() || v.isInt64() || v.isDouble() || v.isBool()) { out = (double) v; return true; }
            return false;
        }

        void setNormalised (juce::RangedAudioParameter& p, float value) { p.setValueNotifyingHost (p.convertTo0to1 (value)); }

        void setByName (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
        {
            if (auto* p = apvts.getParameter (id))
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    setNormalised (*r, value);
        }

        void applyValue (juce::RangedAudioParameter& p, const juce::var& v, Result& r)
        {
            const auto id = p.getParameterID();

            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
            {
                int index = -1;
                double n = 0.0;
                if (v.isString())
                {
                    index = choice->choices.indexOf (v.toString(), true);
                    if (index < 0 && id.endsWith ("_table"))   // a user wavetable can be named by its own name
                        index = WavetableBank::shared().indexOfName (v.toString());
                    if (index < 0)
                        r.warnings.add (id + ": \"" + v.toString() + "\" is not one of [" + choice->choices.joinIntoString (", ") + "]");
                }
                else if (readNumber (v, n))
                    index = juce::roundToInt (n);

                if (index >= 0 && index < choice->choices.size())
                    setNormalised (p, (float) index);
                else if (! v.isString())
                    r.warnings.add (id + ": choice index out of range");
                return;
            }

            double n = 0.0;
            if (! readNumber (v, n))
            {
                r.warnings.add (id + ": expected a number or boolean");
                return;
            }

            const auto range = p.getNormalisableRange();
            if (n < (double) range.start - 1.0e-6 || n > (double) range.end + 1.0e-6)
                r.warnings.add (id + ": " + juce::String (n) + " is outside " + juce::String (range.start) + " .. " + juce::String (range.end) + " (clamped)");

            setNormalised (p, (float) n);
        }

        juce::var nodesToVar (const std::vector<mod::LfoNode>& nodes)
        {
            juce::Array<juce::var> arr;
            for (auto& n : nodes)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("x", std::round (n.x * 10000.0) / 10000.0);
                o->setProperty ("y", std::round (n.y * 10000.0) / 10000.0);
                o->setProperty ("curve", std::round (n.curve * 10000.0) / 10000.0);
                arr.add (juce::var (o));
            }
            return arr;
        }

        bool sameNodes (const std::vector<mod::LfoNode>& a, const std::vector<mod::LfoNode>& b)
        {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (std::abs (a[i].x - b[i].x) > 1.0e-3f || std::abs (a[i].y - b[i].y) > 1.0e-3f || std::abs (a[i].curve - b[i].curve) > 1.0e-3f)
                    return false;
            return true;
        }

        const char* const lfoShapeNames[] { "sine", "triangle", "saw_up", "saw_down", "square", "random" };

        std::vector<mod::LfoNode> defaultShape (int lfo) { return mod::LfoShapes::presetNodes (lfo == 0 ? 0 : lfo == 1 ? 1 : 5); }

        juce::var makeInfoObject (const juce::String& name, const juce::String& category, const juce::String& description, const juce::StringArray& tags,
                                      const juce::String& author, const juce::String& url)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("schema", ember::version::presetSchema);
            o->setProperty ("name", name);
            o->setProperty ("category", category);
            if (author.isNotEmpty()) o->setProperty ("author", author);
            if (url.isNotEmpty())    o->setProperty ("url", url);
            o->setProperty ("description", description);
            juce::Array<juce::var> t;
            for (auto& s : tags) t.add (s);
            o->setProperty ("tags", t);
            return juce::var (o);
        }

        void readInfo (const juce::var& v, Info& info)
        {
            info.name = v.getProperty ("name", {}).toString();
            info.category = v.getProperty ("category", "User").toString();
            info.author = v.getProperty ("author", {}).toString();
            info.url = cleanUrl (v.getProperty ("url", {}).toString());
            info.description = v.getProperty ("description", {}).toString();
            if (auto* arr = v.getProperty ("tags", {}).getArray())
                for (auto& t : *arr) info.tags.add (t.toString());
        }
    }

    bool parse (const juce::String& text, juce::var& out, juce::String& error)
    {
        auto result = juce::JSON::parse (text, out);
        if (result.failed()) { error = result.getErrorMessage(); return false; }
        if (out.getDynamicObject() == nullptr) { error = "top level must be a JSON object"; return false; }
        return true;
    }

    juce::String cleanUrl (const juce::String& text)
    {
        auto t = text.trim();
        if (t.isEmpty() || t.containsAnyOf (" \t\r\n\"<>")) return {};
        if (! t.startsWithIgnoreCase ("http://") && ! t.startsWithIgnoreCase ("https://"))
        {
            if (t.contains ("://") || ! t.contains (".")) return {};
            t = "https://" + t;
        }
        return t;
    }

    juce::String toJson (const juce::var& preset) { return juce::JSON::toString (preset, false, 4); }

    juce::String safeFileName (const juce::String& name)
    {
        return juce::File::createLegalFileName (name.trim()).replaceCharacter ('.', '_');
    }

    // ------------------------------------------------------------------ library
    juce::File Library::userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Emberline").getChildFile ("Spectralis").getChildFile ("Presets");
    }

    static juce::File settingsFile() { return Library::userDirectory().getSiblingFile ("settings.json"); }

    void Library::loadAuthor (juce::String& author, juce::String& url)
    {
        juce::var v;
        if (juce::JSON::parse (settingsFile().loadFileAsString(), v).wasOk())
        {
            author = v.getProperty ("author", {}).toString();
            url = v.getProperty ("url", {}).toString();
        }
    }

    void Library::saveAuthor (const juce::String& author, const juce::String& url)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("author", author);
        o->setProperty ("url", url);
        settingsFile().getParentDirectory().createDirectory();
        settingsFile().replaceWithText (toJson (juce::var (o)), false, false, "\n");
    }

    static int categoryRank (const juce::String& c)
    {
        const int i = categoryOrder.indexOf (c, true);
        return i >= 0 ? i : categoryOrder.size();
    }

    void Library::scanFactory()
    {
        items.clear();
        for (int i = 0; i < PresetData::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = PresetData::getNamedResource (PresetData::namedResourceList[i], size);
            Info info;
            info.factory = true;
            info.json = juce::String::fromUTF8 (data, size);

            juce::var v;
            juce::String error;
            if (! parse (info.json, v, error))
            {
                DBG ("Bad factory preset " << PresetData::namedResourceList[i] << ": " << error);
                continue;
            }
            readInfo (v, info);
            if (info.name.isEmpty())
                continue;
            items.push_back (std::move (info));
        }

        std::stable_sort (items.begin(), items.end(), [] (const Info& a, const Info& b)
        {
            const int ra = categoryRank (a.category), rb = categoryRank (b.category);
            if (ra != rb) return ra < rb;
            if (ra == categoryOrder.size() && a.category != b.category) return a.category < b.category;
            return a.name.compareNatural (b.name) < 0;
        });
        factoryCount = (int) items.size();
    }

    void Library::scanUser()
    {
        items.resize ((size_t) factoryCount);

        std::vector<Info> found;
        for (auto& f : userDirectory().findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            Info info;
            info.file = f;
            info.json = f.loadFileAsString();

            juce::var v;
            juce::String error;
            if (! parse (info.json, v, error))
                continue;
            readInfo (v, info);
            if (info.name.isEmpty())
                info.name = f.getFileNameWithoutExtension();
            if (info.category.isEmpty() || info.category == "User")
                info.category = "User";
            found.push_back (std::move (info));
        }

        std::sort (found.begin(), found.end(), [] (const Info& a, const Info& b) { return a.name.compareNatural (b.name) < 0; });
        for (auto& f : found)
            items.push_back (std::move (f));
    }

    int Library::indexOf (const juce::String& name) const
    {
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].name.equalsIgnoreCase (name))
                return (int) i;
        return -1;
    }

    juce::StringArray Library::categories() const
    {
        juce::StringArray c;
        for (auto& i : items)
            c.addIfNotAlreadyThere (i.category);
        return c;
    }

    // ------------------------------------------------------------------ apply
    Result apply (EmberlineProcessor& proc, const juce::var& preset)
    {
        Result result;
        auto* obj = preset.getDynamicObject();
        if (obj == nullptr)
        {
            result.ok = false;
            result.warnings.add ("preset is not a JSON object");
            return result;
        }
        if ((int) preset.getProperty ("schema", 1) > ember::version::presetSchema)
            result.warnings.add ("preset was made with a newer version of Emberline Spectralis; some settings may be ignored");

        auto& apvts = proc.apvts;

        // 1. everything back to its default (this also clears the modulation matrix)
        for (auto* prm : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());

        // 2. parameters
        if (auto* params = preset.getProperty ("parameters", {}).getDynamicObject())
            for (auto& nv : params->getProperties())
            {
                const auto id = nv.name.toString();
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                    applyValue (*r, nv.value, result);
                else
                    result.warnings.add ("unknown parameter \"" + id + "\"");
            }

        // 3. modulation matrix
        if (auto* mods = preset.getProperty ("modulation", {}).getArray())
        {
            const auto sources = mod::sourceNames();
            int slot = 0;
            for (auto& m : *mods)
            {
                if (slot >= mod::numSlots)
                {
                    result.warnings.add ("more than " + juce::String (mod::numSlots) + " modulation routes (extra ignored)");
                    break;
                }

                int src = -1;
                const auto sv = m.getProperty ("source", {});
                double n = 0.0;
                if (sv.isString()) src = sources.indexOf (sv.toString(), true);
                else if (readNumber (sv, n)) src = juce::roundToInt (n);

                int dst = -1;
                const auto dv = m.getProperty ("destination", {});
                if (dv.isString())
                {
                    const auto& d = mod::destinations();
                    for (size_t i = 0; i < d.size(); ++i)
                        if (d[i].label.equalsIgnoreCase (dv.toString()) || d[i].paramId.equalsIgnoreCase (dv.toString()))
                            dst = (int) i;
                }

                if (src <= 0 || src >= mod::numSources) { result.warnings.add ("modulation: unknown source " + sv.toString()); continue; }
                if (dst < 0)                            { result.warnings.add ("modulation: unknown destination " + dv.toString()); continue; }

                const float depth = (float) (double) m.getProperty ("depth", 0.5);
                const float curve = (float) (double) m.getProperty ("curve", 0.0);
                const bool bipolar = m.hasProperty ("bipolar") ? (bool) m["bipolar"] : mod::isBipolar (src);

                setByName (apvts, mod::slotId (slot, "src"), (float) src);
                setByName (apvts, mod::slotId (slot, "dst"), (float) (dst + 1));
                setByName (apvts, mod::slotId (slot, "depth"), juce::jlimit (-1.0f, 1.0f, depth));
                setByName (apvts, mod::slotId (slot, "curve"), juce::jlimit (-1.0f, 1.0f, curve));
                setByName (apvts, mod::slotId (slot, "bipolar"), bipolar ? 1.0f : 0.0f);
                ++slot;
            }
        }

        // 4. LFO shapes (defaults unless given)
        for (int i = 0; i < mod::numLfos; ++i)
        {
            auto nodes = defaultShape (i);
            const auto sv = preset.getProperty ("lfo_shapes", {}).getProperty ("lfo" + juce::String (i + 1), {});

            if (sv.isString())
            {
                int k = -1;
                for (int s = 0; s < 6; ++s)
                    if (sv.toString().equalsIgnoreCase (lfoShapeNames[s]))
                        k = s;
                if (k >= 0) nodes = mod::LfoShapes::presetNodes (k);
                else result.warnings.add ("lfo" + juce::String (i + 1) + ": unknown shape \"" + sv.toString() + "\"");
            }
            else if (auto* arr = sv.getArray())
            {
                std::vector<mod::LfoNode> custom;
                for (auto& nv : *arr)
                    custom.push_back ({ (float) (double) nv.getProperty ("x", 0.0), (float) (double) nv.getProperty ("y", 0.0),
                                        (float) (double) nv.getProperty ("curve", 0.0) });
                if (custom.size() >= 2) nodes = custom;
                else result.warnings.add ("lfo" + juce::String (i + 1) + ": needs at least 2 nodes");
            }

            proc.lfoShapes.setNodes (i, nodes);
        }

        juce::StringArray macroNames;
        if (auto* arr = preset.getProperty ("macro_names", {}).getArray())
            for (auto& n : *arr) macroNames.add (n.toString());
        apvts.state.setProperty ("macroNames", macroNames.joinIntoString ("|"), nullptr);

        apvts.state.setProperty ("presetName", obj->getProperty ("name").toString(), nullptr);
        apvts.state.setProperty ("presetCategory", obj->getProperty ("category").toString(), nullptr);
        return result;
    }

    // ------------------------------------------------------------------ capture
    juce::var capture (EmberlineProcessor& proc, const juce::String& name, const juce::String& category,
                       const juce::String& description, const juce::StringArray& tags,
                       const juce::String& author, const juce::String& url)
    {
        auto info = makeInfoObject (name, category, description, tags, author, url);
        auto* root = info.getDynamicObject();
        auto& apvts = proc.apvts;

        juce::var paramsVar (new juce::DynamicObject());
        auto* params = paramsVar.getDynamicObject();
        for (auto* prm : proc.getParameters())
        {
            auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
            if (r == nullptr) continue;
            const auto id = r->getParameterID();
            if (id.startsWith ("mod") && juce::CharacterFunctions::isDigit (id[3]))   // slots are written as "modulation"
                continue;
            if (std::abs (r->getValue() - r->getDefaultValue()) < 1.0e-5f)
                continue;

            const float real = r->convertFrom0to1 (r->getValue());
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r))
            {
                const int index = juce::roundToInt (real);
                auto& bank = WavetableBank::shared();
                const bool userTable = id.endsWith ("_table") && index >= bank.numFactoryTables() && index < bank.numSlots() && bank.info (index).loaded;
                // a user wavetable is written by its real name so the preset still works if the files are re-ordered
                params->setProperty (id, userTable ? bank.info (index).name : c->choices[index]);
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (r) != nullptr)
                params->setProperty (id, real > 0.5f);
            else if (dynamic_cast<juce::AudioParameterInt*> (r) != nullptr)
                params->setProperty (id, juce::roundToInt (real));
            else
                params->setProperty (id, std::round ((double) real * 10000.0) / 10000.0);
        }
        const auto macroNames = apvts.state["macroNames"].toString();
        if (macroNames.isNotEmpty())
        {
            juce::Array<juce::var> names;
            for (auto& n : juce::StringArray::fromTokens (macroNames, "|", "")) names.add (n);
            root->setProperty ("macro_names", names);
        }
        root->setProperty ("parameters", paramsVar);

        juce::Array<juce::var> mods;
        const auto sources = mod::sourceNames();
        const auto& dests = mod::destinations();
        for (int s = 0; s < mod::numSlots; ++s)
        {
            const int src = juce::roundToInt (apvts.getRawParameterValue (mod::slotId (s, "src"))->load());
            const int dst = juce::roundToInt (apvts.getRawParameterValue (mod::slotId (s, "dst"))->load()) - 1;
            if (src <= 0 || dst < 0) continue;

            auto* m = new juce::DynamicObject();
            m->setProperty ("source", sources[src]);
            m->setProperty ("destination", dests[(size_t) dst].label);
            m->setProperty ("depth", std::round ((double) apvts.getRawParameterValue (mod::slotId (s, "depth"))->load() * 10000.0) / 10000.0);
            const float curve = apvts.getRawParameterValue (mod::slotId (s, "curve"))->load();
            if (std::abs (curve) > 1.0e-3f) m->setProperty ("curve", std::round ((double) curve * 10000.0) / 10000.0);
            const bool bipolar = apvts.getRawParameterValue (mod::slotId (s, "bipolar"))->load() > 0.5f;
            if (bipolar != mod::isBipolar (src)) m->setProperty ("bipolar", bipolar);
            mods.add (juce::var (m));
        }
        root->setProperty ("modulation", mods);

        juce::var shapesVar (new juce::DynamicObject());   // owned by the var from the start, so nothing leaks if it stays empty
        auto* shapes = shapesVar.getDynamicObject();
        for (int i = 0; i < mod::numLfos; ++i)
        {
            const auto nodes = proc.lfoShapes.getNodes (i);
            if (! sameNodes (nodes, defaultShape (i)))
                shapes->setProperty ("lfo" + juce::String (i + 1), nodesToVar (nodes));
        }
        if (shapes->getProperties().size() > 0)
            root->setProperty ("lfo_shapes", shapesVar);

        return info;
    }
}
