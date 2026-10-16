# Emberline Spectralis - wavetables

A wavetable is a **table of 16 frames**; each frame is one cycle of a waveform. An oscillator picks a table
(`oscN_table`) and a position in it (`oscN_frame`, 0..1), which morphs smoothly between the frames. Like presets,
wavetables are **plain JSON text** that people and LLMs can read, write and edit - and you do not need to rebuild
anything to add one.

* Factory wavetables: `Wavetables/Factory/<Name>.json` (embedded into the plugin when it is built).
  The list, with descriptions, is in [`wavetables.md`](wavetables.md) (generated from the code).
* **User wavetables:** put `.json` or `.wav` files in
  * Windows: `%APPDATA%\Emberline\Spectralis\Wavetables\`
  * Linux: `~/.config/Emberline/Spectralis/Wavetables/`
  * macOS: `~/Library/Application Support/Emberline/Spectralis/Wavetables/`

  The first 8 files (alphabetical) fill the slots `User 1` ... `User 8` in each oscillator's wavetable list.
  The preset menu's *Rescan* entries pick up changes without restarting. Presets refer to a user table by its
  **name** (the `"name"` inside the JSON, or the .wav's file name), so they keep working if you add files.

Every frame is automatically band-limited at 10 levels (mip-mapping), so tables never alias, whatever you write.
Each frame is normalised to a peak of 1.

## Three ways to describe a table

Pick a `"mode"`. Common fields: `name` (required), `category`, `description`, `tags`, and for factory tables a
stable numeric `id`. Optional `harmonic_limit` (1-1023) stops evaluating above that harmonic (faster, and a cleaner
table if you only define a few harmonics).

### 1. `"spectrum"` - a formula for the amplitude of every harmonic

```json
{
    "name": "Saw Sweep", "category": "Analog",
    "description": "A saw with a low-pass filter baked in; Frame opens it.",
    "mode": "spectrum",
    "amplitude": "(1/h)/sqrt(1+pow(h/(1+t*t*70),8))"
}
```

`amplitude` is evaluated for every harmonic `h` = 1, 2, 3 ... of every frame; the value is the level of that
harmonic's **sine** component. Optional `cosine` is the same for the cosine component (use it for phase shifts,
pulse waves and asymmetric shapes). Variables: `h` (harmonic number), `t` (position in the table, 0 on the first
frame to 1 on the last), `frame` (0-15).

### 2. `"waveform"` - a formula for the wave itself

```json
{
    "name": "Sine Fold", "mode": "waveform",
    "expression": "fold(sin(tau*phase)*(1+t*6.5))"
}
```

`expression` is evaluated for each of 2048 samples of every frame. Variables: `phase` (0..1 across one cycle), `t`,
`frame`. The result should lie roughly in -1..1 (it is normalised). It must be one periodic cycle; the engine
removes anything above the playable band for you.

### 3. `"morph"` - cross-fade between stages

```json
{
    "name": "Organ Drawbars", "mode": "morph",
    "stages": [
        { "harmonics": { "1": 1 } },
        { "harmonics": { "1": 1, "2": 0.6 } },
        { "harmonics": { "1": 1, "2": 1, "3": 1, "4": 0.8 } }
    ]
}
```

2-32 `stages` are spread evenly over the 16 frames and linearly cross-faded. A stage is either
* `{ "amplitude": "<expression in h>", "cosine": "<expression in h>" }` (the cosine is optional), or
* `{ "harmonics": [...] }`: a list where item *n* is the amplitude of harmonic *n*, or an object `{ "3": 0.5 }`.
  An entry may also be `[amplitude, phase]` with the phase in cycles (0..1), e.g. `{ "2": [0.7, 0.25] }`.

## Expression reference

Numbers, `+ - * / % ^ ( )`, comparisons `< > <= >= == !=`, logic `&& || !`. Constants `pi`, `tau` (2 pi), `e`.
`^` is power and binds tighter than unary minus (`-2^2` is -4). Division by zero gives 0. No loops, no side effects.

| function | meaning |
|---|---|
| `sin cos tan tanh atan` | trigonometry (radians) |
| `abs sqrt exp log pow(a,b)` | usual meanings (safe: `sqrt`/`log` of a negative give 0) |
| `min(a,b) max(a,b) clamp(x,lo,hi) mix(a,b,t)` | limits and interpolation |
| `floor ceil round frac sign` | rounding; `frac(x)` is the part after the decimal point |
| `if(cond,a,b)` | `a` when cond is true (non-zero), else `b` |
| `odd(h) even(h) prime(h) fib(h)` | 1 if the number is odd / even / prime / a Fibonacci number, else 0 |
| `rand(seed,i)` | deterministic pseudo-random in [0,1): same inputs, same result |
| `bessel(n,x)` | Bessel function J_n(x) |
| `fm(h,ratio,index)` | amplitude of harmonic `h` in two-operator FM (carrier 1, modulator `ratio`, modulation `index`) |
| `fold(x)` | triangle wavefold: 1 for x=1, folds back past +-1 |
| `saw(p) square(p) tri(p) pulse(p,duty)` | naive shapes of a phase `p` (period 1), range -1..1 |
| `bend(p,a) pd(p,a)` | phase warps (bend / Casio-style phase distortion) of a phase `p` by amount `a` 0..1 |

Useful recipes (`h` = harmonic, `t` = position):

* saw `1/h` - square `if(odd(h),1/h,0)` - triangle `if(odd(h),pow(-1,(h-1)/2)/(h*h),0)`
* spectral tilt `1/pow(h, 2-1.5*t)` (dark to bright) - low-pass at harmonic `c`: `1/sqrt(1+pow(h/c,8))`
* a formant / resonant peak at harmonic `c`: `exp(-0.5*pow((h-c)/width,2))`
* comb notches: `0.5+0.5*cos(h*phi)` - FM: `fm(h,2,t*8)`
* sparse random harmonics: `if(rand(7,h) < 0.1+0.8*t, 1/h, 0)`

## Rules of thumb for good tables

* Make **Frame do something audible across the whole range** (the tool warns when the first and last frame are almost identical).
* Think in **harmonic numbers**, not Hz: formants and peaks are fixed *relative to the note*, so they move with pitch.
* Keep the lowest harmonics strong (`h`=1 or 2): tables with no fundamental sound thin and can feel out of tune.
* Use `harmonic_limit` to keep a table smooth and cheap - a table that only defines 40 harmonics is also naturally mellow.
* Frames are cross-faded **linearly in the time domain**, so adjacent frames that are close in shape morph cleanly.

## Real-world audio (.wav)

A `.wav` in the user folder is imported as a wavetable:
* a file whose length is a **multiple of 2048 samples** (the Serum / Vital layout) is read as frames of 2048 samples
  and resampled to the 16 frames (2 or more source frames);
* any other length is read as **one single cycle** of any length and resampled to 2048.

Stereo is mixed to mono. `EmberlineTools export-wavetables <outdir>` writes every factory table as a 16 x 2048 WAV
(32-bit float), so you can use them in other synths or edit them in an audio editor and import them back.

## Workflow

```bash
T=build/EmberlineTools_artefacts/Release/EmberlineTools
$T check-wavetables Wavetables/Factory     # compile every JSON table, report errors with the exact reason
$T check-wavetables ~/my-tables            # ... or your own folder
$T export-wavetables build/wt              # Serum-style WAVs of the factory tables
$T selftest                                 # engine, wavetable and preset regression checks
```

Errors are specific: e.g. `amplitude: unknown name 'x' (available here: h, t, frame) (at character 3)`.

## Prompt template for generating wavetables with an LLM

Give the model this file and, for ideas, `wavetables.md`, then something like:

> You design wavetables for **Emberline Spectralis**. Create a wavetable for: **"<describe the timbre and how it should
> change from the first frame to the last>"**.
>
> Rules:
> 1. Output **one strict JSON object** (no comments, no markdown) following `WAVETABLES.md`.
> 2. Choose `"mode"`: `"spectrum"` for harmonic recipes, `"waveform"` for waveshaping/sync/fold effects, `"morph"` to
>    blend between a few distinct sounds.
> 3. Use only the variables and functions listed in the expression reference. `t` runs 0..1 across the table, so
>    make the sound change across it.
> 4. Include `name`, `category` (Analog, Digital, FM, Vocal, Organic), a one-sentence `description` of what Frame does, and 3-5 `tags`.
> 5. Keep harmonic 1 strong, avoid DC, and set `harmonic_limit` when you only define the low harmonics.

Save it as a `.json` file in the user wavetable folder (or `Wavetables/Factory` plus a rebuild), run
`EmberlineTools check-wavetables <folder>`, then select it from an oscillator's wavetable list.

## 4. `"wav"` - recorded or sampled material

```json
{ "name": "Clavinet", "category": "Sampled", "mode": "wav", "file": "Clavinet.wav", "frame_size": 2048, "align_phase": true }
```

* `frame_size` (default 2048): length of one cycle in the file; the file is split into cycles and resampled to 16 frames.
* `align_phase` (default true): cycles are phase-aligned and interpolated spectrally, so morphs do not comb-filter.
* **Dropping a `.wav` straight into the user folder works too** (or use the *Import* button / *Rescan* in the table menu).

### WAV file specification (for contributors)

| | |
|---|---|
| Format | PCM WAV, 16/24-bit or 32-bit float, mono (stereo is folded to mono) |
| Layout A (Serum style) | N consecutive cycles, each exactly **2048 samples**; N from 1 to 256 (file length is a multiple of 2048) |
| Layout B (single cycle) | One cycle of any length (64 - 16384 samples); it is resampled to 2048 and repeated across the frames |
| Content | Each cycle should start and end near zero crossing with matching slope; no DC; peak below 0 dBFS |
| Names | `Descriptive Name.wav`; no `:` or `/`; the name is shown in the table menu |
| Licence | Only material you made or that is CC0 / public domain |
