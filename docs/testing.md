# Manual test checklist

Automated checks (`EmberlineTools selftest`, `check`, `perf`, CI, pluginval) cover the DSP, presets, saved state and arp logic.
This list covers what only a person with a real host, mouse and ears can check. Work through it on each platform / host
before a beta tag, and note the build (`vX.Y.Z pre-beta`, shown under the preset bar) and OS at the top of your results.

Mark each line **P**ass, **F**ail (add a note) or **-** not applicable. File failures as issues with the host, OS and steps.

## 0. Setup

- [ ] Fresh build from a clean clone of the repo (`scripts\build-windows.bat` / CI artifact on Mac).
- [ ] VST3 copied to the VST3 folder; Standalone launches with no error dialog.
- [ ] Version label under the preset bar matches the tag.

## 1. Hosts (repeat the core list per host)

Hosts to cover: Reaper, Ableton Live, FL Studio (Windows); Ableton Live, Logic or Reaper (Mac, Logic needs AU so check
the VST3 hosts first); plus the Standalone.

| check | Reaper | Ableton | FL Studio | Mac host | Standalone |
|---|---|---|---|---|---|
| plugin scans and appears as an instrument | | | | | |
| opens GUI at the right size, no blank/garbled drawing | | | | | |
| plays MIDI, correct pitch, no stuck notes on stop | | | | | |
| 8 voices, no crackle at 64 / 256 / 1024 buffers | | | | | |
| save project, close, reopen: sound and preset name restored | | | | | |
| automate a knob from the host (and it moves on screen) | | | | | |
| two instances at once, independent sounds | | | | | |
| change sample rate (44.1 / 48 / 96 kHz): no pitch change, no glitches | | | | | |
| close the GUI with notes playing, reopen: fine | | | | | |
| remove the plugin from the track: no crash | | | | | |

## 2. Sound

- [ ] Default patch ("Honeyed Keys") is audible, not harsh, not clipping.
- [ ] **Drift** 0 -> 1: pitch wanders more; **Warmth** 0 -> 1: gets softer and darker. Both at 0 = the old clean sound.
- [ ] Filter types all work: Lowpass, Highpass, Bandpass, Formant, Notch, Ladder. 12 vs 24 dB audibly steeper.
- [ ] Ladder at high resonance self-limits and does not blow up (turn cutoff quickly).
- [ ] Unison 1 -> 16 stays in tune, no level jump that surprises.
- [ ] Audition ~20 presets across categories with `docs/audition-sheet.csv`; write notes (too quiet / harsh / boring / great).
- [ ] No clicks on preset change while notes sound, or on switching oscillator tables.

## 3. Interface (mouse)

- [ ] Every knob: drag, fine-drag (hold Shift/Ctrl), double-click to reset, value box edit.
- [ ] **Mod chips -> knobs:** drag a chip (LFO 1, Env 1, Macro 1...) onto a knob. The route popup opens in place.
- [ ] In the popup: depth slider, curve, bipolar, remove. Removing the last route closes the popup. Dragging the same chip again adds nothing odd.
- [ ] Right-click a modulated knob re-opens the popup. Right-click a plain knob does nothing harmful.
- [ ] The knob ring shows the modulation range and animates.
- [ ] Mod Matrix tab shows the same routes you made; edits there match the knobs.
- [ ] Each OSC panel has its own mesh view that follows frame / warp / table changes.
- [ ] Visualizers tab: filter-response curve moves with cutoff / resonance, shows modulation, serial vs parallel.
- [ ] All 6 tabs open; nothing overlaps at the default size; text readable.
- [ ] Resize / DPI scaling 100 %, 125 %, 150 % (Windows): nothing clipped or blurry.
- [ ] The **?** button shows a menu: *Open the manual* opens it in the default browser (loads, sidebar and links work, screenshots show, no network needed); *Support EmberlineSky* opens buymeacoffee.com/emberlinesky. The dim "buy me a coffee" line under the credit does the same.
- [ ] Tooltip on the author credit appears; the credit does not block the preset bar.

## 4. Presets

- [ ] `<` `>` step through all presets; the menu lists every category.
- [ ] Header shows **by <author>** and the link; clicking it opens the browser at the right page (EmberlineSky -> Instagram).
- [ ] **Preset info...** shows author, link, description, tags; **Open link** works.
- [ ] **SAVE**: all fields (name, category, description, tags, author, link) end up in the file in the user preset folder
      (menu > Show user preset folder). Author and link are remembered for next time.
- [ ] Saved preset reloads identically, appears under its category, and keeps its credit.
- [ ] A link typed without `https://` is fixed; a bad link (`javascript:...`, spaces) is dropped.
- [ ] **Export current sound as .json...** writes a file anywhere; a preset using a non-factory wavetable shows the warning.
- [ ] **Import preset file(s)...**: valid files appear; a text file renamed `.json` is skipped with a message; importing the same file twice does not overwrite.
- [ ] Hand-edit a preset JSON (change a value), Rescan user presets: change is picked up. Break the JSON: it is ignored, no crash.

## 5. Wavetables

- [ ] Drop a WAV single-cycle file in the user wavetable folder (`docs/WAVETABLES.md`); it appears in a "User n" slot after rescan.
- [ ] Thumbnails look right; selecting it plays without clicks.

## 5b. Mono, legato and glide (FILTERS & ENVS > VOICE, and the GLIDE knob in Master)

- [ ] Poly (default): chords play normally. Glide up: each new note slides from the previous note's pitch.
- [ ] Mono: only one note at a time, newest wins; every note retriggers the envelope. Release the top note while holding another: it falls back.
- [ ] Legato: a note played while another is held changes pitch without retriggering the envelope; a detached note retriggers.
- [ ] Glide time 0 -> 3 s: slide gets slower, always lands exactly on the played note. Glide mode "Legato Only": detached notes don't slide.
- [ ] Switching mode while holding a note leaves nothing hanging. Works with the arp on (mono arp lines glide between steps).
- [ ] Presets: Tidal Slide, Moon Drop, Sunrise Slide, Unalived Rat feel right on a keyboard. Sustain pedal in Mono/Legato is ignored (poly only).

## 6. Arpeggiator and stutter (ARP & STUTTER tab)

- [ ] Arp on, hold a chord: it plays; releasing stops it (latch off). Latch on: keeps playing, next chord replaces it.
- [ ] All 10 modes sound as named; octaves 1-4; the 16-step pattern bars follow the pattern.
- [ ] Rate, gate, swing, accent, humanise audibly work.
- [ ] **Host sync:** play the DAW transport; the arp stays on the grid and follows tempo changes and loop jumps. Stop transport: free-runs without stuck notes.
- [ ] Stutter modes: speeds up / slows down / up-and-down over the chosen length; Smooth vs Stepped; velocity swell.
- [ ] Stutter display matches what you hear. Turning the arp off mid-note leaves nothing hanging.
- [ ] The 7 arp factory presets (Rolling Gold, Offbeat Orchard, Stutter Rise, Ball Drop, Heartbeat Sway, Pulse Garden Stutter, Moonlit Cascade) feel right.

## 7. Stress and edge cases

- [ ] Hold 8+ notes and slam all the heavy knobs: no dropouts at a 256-sample buffer (check the host CPU meter; `docs/performance.md`).
- [ ] Rapid preset switching with notes held: no crash, no stuck sound.
- [ ] Sustain pedal, pitch bend, mod wheel, aftertouch all do something sensible.
- [ ] Open the project on a different machine without your user presets / wavetables: it loads, with a fallback sound rather than a crash.
- [ ] Run the Standalone for 30 minutes with a held note: no growth in memory, no drift in tuning.

## Results log

| date | build | OS / host | tester | notes / issues |
|---|---|---|---|---|
| | | | | |
