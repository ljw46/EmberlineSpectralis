# Emberline Spectralis - presets

Presets are plain **JSON text files**. They are meant to be read, edited and *written* by people and by LLMs.
The same format is used for the factory library, for user presets, and for presets you generate from a prompt.

* Factory presets: `Presets/Factory/<Category> - <Name>.json` (embedded into the plugin at build time)
* User presets: saved by the **SAVE** button into
  * Windows: `%APPDATA%\Emberline\Spectralis\Presets\`
  * Linux: `~/.config/Emberline/Spectralis/Presets/`
  * macOS: `~/Library/Application Support/Emberline/Spectralis/Presets/`
  Drop any `.json` file in there (menu: *Rescan user presets*) and it appears under **USER**.
* The full list of parameters, ranges, defaults and choices is in [`parameters.md`](parameters.md) (generated from the
  code, so it is always accurate - regenerate with `EmberlineTools dump-params docs`).

## File format

```json
{
    "name": "Amber Hour",
    "category": "Pad",
    "description": "One or two sentences: what it sounds like and when to use it.",
    "tags": ["pad", "warm", "melodic house"],
    "macro_names": ["Brightness", "Morph", "Resonance", "Width"],
    "parameters": {
        "osc1_frame": 0.45,
        "osc1_unison": 4,
        "filt1_type": "Lowpass",
        "filt1_cutoff": 2400,
        "fx_reverb_on": true,
        "fx_reverb_mix": 0.35
    },
    "modulation": [
        { "source": "LFO 1", "destination": "Filt1 Cutoff", "depth": 0.06 },
        { "source": "Macro 1", "destination": "Filt1 Cutoff", "depth": 0.5 }
    ],
    "lfo_shapes": { "lfo1": "triangle" }
}
```

Strict JSON: no comments, no trailing commas. Only `name` and `category` are required.

| field | meaning |
|---|---|
| `name`, `category` | shown in the preset browser. Categories: `Init Bass Pluck Lead Pad Keys Chord Arp Organic Texture FX` (anything else appears after them; saved user presets use `User`). |
| `description`, `tags` | free text, for humans and search. |
| `macro_names` | labels for Macro 1-4 under the knobs (use `"-"` for an unused macro). |
| `parameters` | **only the values that differ from the defaults**. Everything not listed resets to its default - a preset never inherits from the previous sound. |
| `modulation` | up to **16** routes, applied in order. |
| `lfo_shapes` | optional; LFO 1/2/3 default to sine / triangle / random steps. |

### Parameter values

Keys are the ids in `parameters.md`. Values are **real units**, not 0-1:

* floats: the real number (`"filt1_cutoff": 2400` is 2400 Hz, `"master_volume": -3.5` is -3.5 dB)
* ints: whole numbers (`"osc1_unison": 4`, `"osc1_semi": -12`)
* bools: `true` / `false` (`"osc2_on": true`)
* choices: the **name** as a string, case-insensitive (`"filt1_type": "Formant"`, `"fx_delay_sync": "1/8."`)

Loading is lenient: an unknown id, a bad choice name or an out-of-range number produces a *warning* (the value is
clamped or skipped) and the rest of the preset still loads. `EmberlineTools check` prints every warning.

### Modulation routes

```json
{ "source": "ENV 2", "destination": "Filt1 Cutoff", "depth": 0.3, "curve": 0.0, "bipolar": false }
```

* `source`: `LFO 1-3`, `ENV 1-3`, `Macro 1-4`, `Chaos X`, `Chaos Y`, `Chaos Z`, `Perlin`, `S&H`, plus performance sources `Velocity`, `Key Track`, `Mod Wheel`, `Aftertouch`, `Random`.
  Only global sources (LFO, Macro, Chaos, Mod Wheel, Aftertouch) can drive the FX destinations (reverb/delay/chorus/distortion amounts).
  LFO shapes can be step patterns (`lfo_shapes`), handy for tempo-locked gating and pumping; `pitch_bend_range` sets the wheel range.
* `destination`: the label (`Filt1 Cutoff`, `Osc2 Frame`, `Osc1 Pan`, ...) or the parameter id (`filt1_cutoff`) - full list in `parameters.md`
* `depth` (-1..1): how far the source moves the destination, **as a fraction of the destination's whole range**
  (in the same skewed 0-1 scale the knob uses).
* `curve` (-1..1, default 0): shapes the source before the depth is applied. `+` = fast then easing (snappy), `-` = slow start.
* `bipolar` (default = the source's natural polarity): LFOs and chaos are bipolar (-1..1); envelopes and macros are
  unipolar (0..1). `bipolar: true` on a unipolar source maps it to -1..1; `false` on a bipolar source maps it to 0..1.

**Filter-cutoff depth is not linear in Hz.** The cutoff knob is skewed, so work in the normalised scale
`n(f) = ((f - 20) / 19980) ^ 0.25` and use `depth = n(target) - n(base)`:

| cutoff | n |
|---|---|
| 100 Hz | 0.252 |
| 200 Hz | 0.308 |
| 300 Hz | 0.344 |
| 500 Hz | 0.394 |
| 800 Hz | 0.445 |
| 1000 Hz | 0.471 |
| 1500 Hz | 0.522 |
| 2000 Hz | 0.561 |
| 3000 Hz | 0.621 |
| 5000 Hz | 0.707 |
| 8000 Hz | 0.795 |
| 12000 Hz | 0.880 |
| 16000 Hz | 0.946 |
| 20000 Hz | 1.000 |

Example: an envelope that opens a filter from 1100 Hz (n = 0.482) to 7000 Hz (n = 0.769) at its peak needs
`depth = 0.769 - 0.482 = 0.287`. The generator script has a `sweep(base, to)` helper that does this.

Useful rules of thumb (all depths are fractions of the destination range):
* Osc frame / warp amount / level / spread / pan: range is 0..1 (pan -1..1), so depth is directly the amount.
* Detune: 0..100 cents but skewed - small depths already give a lot.
* Osc fine tune: range is +-100 cents, so **vibrato** of +-6 cents is `depth 0.03` from an LFO.

### LFO shapes

`"lfo_shapes": { "lfo1": "sine" }` - named: `sine`, `triangle`, `saw_up`, `saw_down`, `square`, `random`. Or custom nodes
(x 0..1 ascending with the first at 0 and the last at 1, y -1..1, `curve` -1..1 shapes the segment *leaving* that node):

```json
"lfo_shapes": { "lfo2": [ {"x":0,"y":-1,"curve":0}, {"x":0.3,"y":1,"curve":0.5}, {"x":1,"y":-1,"curve":0} ] }
```

### Tempo sync

`lfoN_sync` and `fx_delay_sync` take: `Off`, `1/16T`, `1/16`, `1/16.`, `1/8T`, `1/8`, `1/8.`, `1/4T`, `1/4`, `1/4.`, `1/2`,
`1 bar`, `2 bars`, `4 bars` (`T` = triplet, `.` = dotted). With sync on, the rate/time knob is ignored. LFOs only
lock to the host's bar position when `lfoN_mode` is `"Free"` and the host is playing. Without a host (standalone) the
`global_tempo` parameter provides the BPM (default 120).

**Sidechain-style pumping** and **gating** are built from this: set `lfo3_mode: "Free"`, `lfo3_sync: "1/4"`,
`lfo_shapes.lfo3: "saw_up"` and route `LFO 3` to `Osc1 Level` (and Osc2/3) with depth ~0.4-0.55 (pump), or use
`"square"` with `"1/16"` (gate).

### FX chain

Order is `fx_slot1..4` (each holds one of `Distortion`, `Chorus/Flanger`, `Delay`, `Reverb`; default is that order).
Each effect has its own `fx_*_on` switch and a mix. Only listed effects run.

## Sound-design guide for this engine

**Signal path:** 3 wavetable oscillators (+ noise) -> Filter 1 / Filter 2 (serial or parallel) -> + sub oscillator (bypasses
the filters) -> amp **ENV 1** -> FX chain -> master volume. ENV 1 is the amp envelope. ENV 2/3 are free modulators.
A note ends when ENV 1 finishes - sustain 0 gives a pluck that dies after the decay.

**Wavetable (`oscN_table`) and position (`oscN_frame`, 0..1):** `oscN_table` picks one of the wavetables (by name - see
[`wavetables.md`](wavetables.md); `Classic Morph` is the default) and `oscN_frame` moves through its 16 frames. What Frame
does depends on the table: in `Classic Morph` 0 = sine, 0.25 = triangle, 0.5 = saw, 0.75 = square, 1.0 = reedy; in `Vowel Sweep`
it glides A-E-I-O-U; in `FM 1:1` it raises the modulation index; in `Pulse Width` it narrows the pulse; and so on. Modulate
`Frame` (LFO / envelope / macro / chaos) for the classic wavetable movement. Choose the table for the *character* first,
then set Frame. You can write new tables yourself - see [`WAVETABLES.md`](WAVETABLES.md).

**Warp (`oscN_warp_mode` + `oscN_warp_amt`):** `Formant` (vocal, nasal), `Sync` (cutting lead, laser), `Bend` (asymmetric
growl), `Phase Dist` (digital, pulse-width-like). Animate the amount with an envelope or LFO for movement.

**Unison (`oscN_unison` 1-16, `_detune` cents, `_spread` 0-1):** 1 = mono/clean (basses, subs). 3-5 voices, detune 10-18 =
warm and wide (pads, plucks, keys). 6-7 voices, detune 20+ = supersaw. CPU grows with voices x oscillators.

**Filters:** `Lowpass` for almost everything. `Highpass` for air/noise beds. `Bandpass` for hollow/woody/clav sounds.
`Formant` turns the cutoff into a vowel position (low = "oo", high = "ee"): sweep it for vocal movement. Resonance 0.1-0.3
is musical, 0.6+ squelches. `filt_drive` adds saturation.

**Envelopes:** plucks `attack 0.001-0.005, decay 0.2-0.6, sustain 0`. Keys `decay 0.6-1.2, sustain 0.2-0.4`.
Pads `attack 0.6-2.5, release 1.5-4`. Basses `attack 0.002-0.005, sustain 0.6-0.9, release 0.1-0.2`. `dcurve` > 0 = snappy.
Filter envelope on ENV 2: route `ENV 2` -> `Filt1 Cutoff` and give ENV 2 `sustain 0`.

**Movement:** slow LFOs (0.05-0.3 Hz, `Free`) on cutoff / frame / pan keep pads alive. `Perlin` is smooth organic drift;
`S&H` is stepped random; `Chaos X/Y/Z` are the Lorenz attractor (good for stereo wander).

**FX recipes:** reverb `mix 0.2-0.4, decay 2.5-6 s` (pads/FX up to 8-12 s); chorus `mix 0.3-0.5` for width; delay
`1/8.` with `feedback 0.35-0.5` for melodic house; `Tube` or `Soft` distortion for warmth, `Hard`/`Fold` for aggression.

**Macros (convention used by the factory library):** Macro 1 = brightness (-> `Filt1 Cutoff`), Macro 2 = morph
(-> `Osc1 Frame`), Macro 3 = resonance (-> `Filt1 Res`), Macro 4 = width (-> `Osc1 Detune`). Give them meaningful
`macro_names`. Always leave Macros at 0 in the preset itself (their knob value is a performance control).

**Loudness:** presets are level-matched with the tool (`--fix-levels`), which sets `master_volume`. When you write a
preset by hand, leave `master_volume` out and run the tool afterwards.

## Workflow

```bash
# build the dev tool once (not part of the plugin)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target EmberlineTools

T=build/EmberlineTools_artefacts/Release/EmberlineTools
$T check Presets/Factory                       # validate + measure peak / loudness, report warnings
$T check Presets/Factory --fix-levels          # ...and write a level-matched master_volume into each file
$T check Presets/Factory --wav previews/       # ...and export a WAV preview of every preset to audition
$T render "Presets/Factory/Pad - Amber Hour.json" amber.wav
$T selftest                                     # engine + preset regression checks
$T dump-params docs                             # regenerate parameters.md / parameters.json
```

`EmberlineTools check` renders every preset (a held chord or note, chosen per category, then the tail) and reports
`peak dBFS`, loudness and problems (`SILENT`, `CLIPS`, `NON-FINITE OUTPUT`, `WARN ...`). Factory presets are embedded when
the plugin is built, so rebuild after editing files in `Presets/Factory`.

## Prompt template for generating presets with an LLM

Give the model `docs/parameters.md` and this file, then something like:

> You are a synthesizer sound designer for **Emberline Spectralis**, a wavetable synth for melodic, deep and organic house.
> Create a preset for the following sound: **"<describe the sound, its role, mood, reference points>"**.
>
> Rules:
> 1. Output **one strict JSON object** (no comments, no markdown, no trailing commas) in the format of `PRESETS.md`.
> 2. Use only parameter ids, choice names and modulation sources/destinations that appear in `parameters.md`.
> 3. List only parameters that differ from their defaults. Use real units. Do not set `master_volume`.
> 4. Pick `category` from: Bass, Pluck, Lead, Pad, Keys, Chord, Arp, Organic, Texture, FX.
> 5. Write a one-to-two sentence `description`, 3-6 `tags`, and four `macro_names`.
> 6. Use at most 16 modulation routes. Compute filter-cutoff depths with `n(f) = ((f-20)/19980)^0.25`.
> 7. Make the amp envelope (ENV 1) and filter movement fit the sound's role (see the sound-design guide).
> 8. Keep Macros at 0; route Macros 1-4 to useful performance targets.

Save the result as `Presets/Factory/<Category> - <Name>.json` (or into the user preset folder), run
`EmberlineTools check ... --fix-levels`, and listen.
