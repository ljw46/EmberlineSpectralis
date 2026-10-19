# Filters, drift and warmth

## The filters

There are **two filters** (on the FILTERS & ENVS tab). By default only Filter 1 is on, as a lowpass.

### Filter types

| Type | Sounds like |
|---|---|
| **Lowpass** | Cuts the top end. The one you'll use most: pads, bass, plucks. |
| **Highpass** | Cuts the low end. Thins things out, good for risers. |
| **Bandpass** | Only lets a slice through. Nasal, vocal, "phone" sounds. |
| **Notch** | Scoops a slice out. Phasey, hollow. |
| **Formant** | Vowel-like filtering. Talky, vocal. |
| **Ladder** | A warm, fat, analogue-style 4-pole lowpass. It bites when you push the resonance, then saturates nicely. |
| **Off** | Bypass. |

If your sound feels a bit harsh or metallic, try **Ladder**. It's the quickest way to get it to sit nicer.

### Slope: 12 dB or 24 dB

Next to the type there's a **12 dB / 24 dB** switch. It's how steeply the filter cuts. 12 dB is gentle and open;
24 dB is tighter, darker and more obviously "filtery". If you're not sure, try both and trust your ears.

### The knobs

- **CUTOFF** - where the filter starts to cut (20 Hz to 20 kHz).
- **RESONANCE** - a boost right at the cutoff. A little adds character; a lot whistles.
- **DRIVE** - pushes more signal into the filter for some grit.

You can also grab **CUTOFF** and **RESONANCE** for Filter 1 in the Master panel, so they're always in reach.

### Two filters together: Routing

Use the **Routing** box (in the **VOICE / ROUTING** panel on the right of the Filters tab):

- **Serial** - the sound goes through Filter 1, then Filter 2. Great for a lowpass plus a highpass (a band you can squeeze).
- **Parallel** - both filters work on the original sound, then get mixed. Great for two bands, or a notch + something else.

### Watching the filter work

The **VISUALIZERS** tab has a big live picture of the filter curve. You'll see it open and close as the cutoff moves,
including anything you've got modulating it, the curve for each filter, and little markers showing the cutoff.
It follows the latest note you played, so play a note and watch.

![Filter display](img/1-visualizers.png)

## Drift and Warmth (the "analogue" bit)

Digital synths can sound a bit too perfect. Two knobs in the **Master** panel fix that:

**DRIFT** - makes the pitch wander very slightly, and makes each note a tiny bit different, like old analogue gear that
never quite stays still. At low settings you'll feel it more than hear it: a bit of life and width. At high settings
it gets properly wobbly.

**WARMTH** - gently squashes the loudest peaks (like pushing a tape or a transformer) and rolls off the harsh top end.
Turn it up for a softer, darker, fatter sound.

Try this: play a pad, then slowly turn Warmth up. Hear the fizzy top end relax? That's the sweet spot for a deep house chord.

Both are **on a little by default** (Drift 0.25, Warmth 0.3), so everything sounds a bit warmer out of the box.
Too dull? Back off Warmth. Want it totally clean and crisp? Set both to **0**.

Presets remember their own Drift and Warmth settings, so you'll notice some sounds are clean and some are dirtier.
That's on purpose.
