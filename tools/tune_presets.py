#!/usr/bin/env python3
"""Post-generation tuning pass for the factory presets (idempotent: safe to run again).

    python3 tools/tune_presets.py            # edits Presets/Factory/*.json in place
    EmberlineTools check Presets/Factory --fix-levels   # then re-level (warmth changes loudness)

What it does:
  1. gives every preset its own Drift / Warmth (by category, plus a small repeatable per-preset variation) unless the
     preset already sets them, so the two knobs are no longer the same on every sound;
  2. tames the presets that measured as harsh (very bright for what they are) by rounding the top end;
  3. replaces the noisy "Crackle" settings on the rain / fire presets with something that reads as rain / fire;
  4. makes the Init preset a soft starting point instead of a raw saw into a wide-open filter.
"""
import glob, json, os, sys, zlib

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
FACTORY = os.path.join(ROOT, "Presets", "Factory")

# category -> (drift, warmth). Pads / textures wander more; plucks and keys get the clearer warmth; bass stays steadier.
BASE = {
    "Pad": (0.45, 0.45), "Keys": (0.35, 0.50), "Pluck": (0.20, 0.55), "Lead": (0.30, 0.45), "Bass": (0.12, 0.45),
    "Arp": (0.20, 0.40), "Chord": (0.35, 0.45), "Texture": (0.50, 0.40), "Organic": (0.45, 0.50), "FX": (0.25, 0.30),
}

# presets that measured as harsh (very bright for what they are): absolute lowpass cutoff (Hz) and warmth
HARSH = {
    "Lead - Glass Hymn":          dict(cutoff=4400, warmth=0.70),   # was 8000
    "Lead - Lasers In The Fog":   dict(cutoff=3600, warmth=0.70),   # was 6000
    "Lead - Neon Silk":           dict(cutoff=3600, warmth=0.65),   # was 5200
    "Lead - Prism Lead":          dict(cutoff=4400, warmth=0.60),   # was 6500
    "Lead - Spire":               dict(cutoff=5200, warmth=0.65),   # was 8500
    "Arp - Glass Staircase":      dict(cutoff=4400, warmth=0.55),   # was 6500
    "Arp - Rolling Gold":         dict(cutoff=4000, warmth=0.55),   # was 6000
    "Texture - Electric Orchard": dict(cutoff=4400, warmth=0.60),   # was 6500
}


# presets whose master volume is already at its maximum: keep warmth low so the extra softening does not make them quiet
LOW_WARMTH = {"Arp - Staccato Choir": 0.15}


def jitter(name, salt):
    """Repeatable value in -0.06 .. +0.06 from the preset name, so presets in one category are not identical."""
    return ((zlib.crc32((name + salt).encode()) % 1000) / 1000.0 - 0.5) * 0.12


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def tune(name, d):
    p = d["parameters"]
    cat = d.get("category", "")
    if cat in BASE:
        drift, warm = BASE[cat]
        p.setdefault("analog_drift", round(clamp(drift + jitter(name, "d")), 2))
        p.setdefault("analog_warmth", round(clamp(warm + jitter(name, "w")), 2))

    if name in LOW_WARMTH:
        p["analog_warmth"] = LOW_WARMTH[name]

    if name in HARSH:
        p["filt1_cutoff"] = HARSH[name]["cutoff"]
        p["analog_warmth"] = HARSH[name]["warmth"]

    if name == "Organic - Rain On Leaves":
        # steady shower: dense pink noise through a high band, droplets from the bell, instead of 250 clicks a second
        p.update(noise_type="Pink", noise_level=0.55, filt1_type="Highpass", filt1_cutoff=1400, filt1_res=0.05,
                 fx_reverb_mix=0.4)
        d["description"] = ("A steady, soft shower on leaves: hushed filtered noise with a faint, high bell tone floating above it "
                            "like far-off droplets. Fresh and calm - drop it under anything for instant atmosphere.")
    if name == "Organic - Campfire":
        # fire is mostly a low warm bed with the odd pop, so the crackle comes down a lot and the top end is rounded
        p.update(noise_type="Crackle", noise_level=0.3, filt1_cutoff=1800)
    return d


def dumps(d):
    """Same layout as the generator: header keys inline, parameters and modulation one entry per line."""
    out, keys = ["{"], list(d)
    for n, k in enumerate(keys):
        comma = "," if n < len(keys) - 1 else ""
        v = d[k]
        if k == "parameters" and v:
            items = list(v.items())
            out.append('    "parameters": {')
            out += [f'        "{a}": {json.dumps(b)}' + ("," if i < len(items) - 1 else "") for i, (a, b) in enumerate(items)]
            out.append("    }" + comma)
        elif k == "modulation" and v:
            out.append('    "modulation": [')
            out += ["        " + json.dumps(m) + ("," if i < len(v) - 1 else "") for i, m in enumerate(v)]
            out.append("    ]" + comma)
        else:
            out.append(f'    "{k}": {json.dumps(v, ensure_ascii=False)}' + comma)
    out.append("}")
    return "\n".join(out) + "\n"


def main():
    changed = 0
    for f in sorted(glob.glob(os.path.join(FACTORY, "*.json"))):
        d = json.load(open(f, encoding="utf-8"))
        name = os.path.basename(f)[:-5]
        raw = open(f, encoding="utf-8").read()
        if name == "Init - Basic Saw":
            d["parameters"].update(filt1_cutoff=3200.0, filt1_res=0.1, analog_drift=0.3, analog_warmth=0.5,
                                   env1_attack=0.005, env1_release=0.3)
            d["description"] = ("A soft starting point: one saw-style oscillator through a gentle lowpass with a little drift and "
                                "warmth. Open the filter for brightness, or reset knobs to taste.")
        else:
            d = tune(name, d)
        text = dumps(d)
        if text != raw:
            with open(f, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)
            changed += 1
    print(f"tuned {changed} presets")


if __name__ == "__main__":
    sys.exit(main())
