# Recipes

Starting points to bend into your own thing. Numbers are a guide: trust your ears, not the numbers.

All of these start from the default sound; use **Init - Basic Saw** (in the Init category) if you'd like a blank page.

## Warm deep-house chord pad

1. OSC 1: pick **Saw Sweep** or **Blended Waves**. **UNISON 4**, **DETUNE** around 12, **SPREAD** 0.6.
2. Switch OSC 2 on, same table, **SEMI** 0, **DETUNE** a touch different.
3. Filter 1: **Ladder**, **24 dB**, **CUTOFF** about 2.5 kHz, **RESONANCE** low.
4. **ENV 1**: slow **ATK** (0.3 s), long **REL** (1.2 s).
5. Drag **LFO 1** onto Filter 1 **CUTOFF**, small **DEPTH**, **RATE** synced to 1 bar for a slow breathe.
6. FX: a touch of **Chorus**, then **Reverb** with **DECAY** about 5 s. Turn **WARMTH** to about 0.4.

## Rolling sub bass

1. OSC 1: **Digital Bass** or **Bass Weight**, **UNISON 1**.
2. **Sub** at about 0.5 (sine, -1 Oct).
3. Filter 1: **Lowpass**, **24 dB**, **CUTOFF** low (about 400 Hz).
4. **ENV 2** onto **CUTOFF** for a short pluck on each note (short **DEC**, **SUS** 0).
5. Short **REL** on ENV 1 for a tight bass.
6. Add a touch of Distortion (**Tube**) for presence.

## The pitch-slide bassline

1. Voice panel: **Legato**, glide mode **Legato Only**, **GLIDE** about 0.3 s.
2. OSC 1: **Bass Weight**; **Sub** about 0.4; Filter 1 **Ladder**, **24 dB**, **CUTOFF** around 400 Hz.
3. Play a long, slow line and overlap each note into the next. Short, separate notes stay punchy; overlapped ones slide.
4. For a bigger swoop use **Mono** with **GLIDE** at 0.5 s or more and glide mode **Always**.
5. Try it with the arp: set Mono and a slow rate, and the line glides between steps.

## Glassy pluck

1. OSC 1: **Mallet** or **Glass Harmonics**.
2. **ENV 1**: short **DEC** (0.3 s), **SUS** 0.
3. Filter 1: **Lowpass**, **CUTOFF** about 5 kHz, **ENV 2** onto cutoff, small depth.
4. Delay: sync **1/8.**, **FEEDBACK** about 0.4, **MIX** about 0.25. Reverb on the side.

## Evolving organic lead

1. OSC 1: **Flute**, **Cello** or **Human Voice**.
2. Drag **Perlin** onto OSC 1 **FRAME** and onto **FINE** with a tiny depth.
3. **Velocity** onto Filter 1 **CUTOFF**, so harder notes are brighter.
4. Mod Wheel onto **Reverb Mix** for a swell.

## Rolling arp with a stutter build

1. Load any pluck or keys sound. Go to **ARP & STUTTER**, tick **ON**.
2. Mode **Up**, **1/16**, Pattern **Rolling**, **OCTAVES** 2.
3. Stutter mode **Speed Up**, Length **2 bars**, Curve **Stepped**, **DEPTH** 0.5.
4. Hold a chord for 8 bars and automate stutter **Mode** to Off for the drop.

## One-knob build-up

1. Pick **M1**, and drag its chip onto Filter 1 **CUTOFF**, Reverb **Mix** and Delay **Mix** (right-click each knob to set depths).
2. Automate **M1** in your DAW from 0 to 1 over 8 bars.
