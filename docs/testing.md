# Testing

Automated checks (`EmberlineTools selftest`, `check`, `perf`, CI, pluginval at strictness 5 on all three OSes) cover the DSP, presets,
saved state, arp logic and installer packaging. What only a person with a real host, mouse and ears can check is listed here at a
high level. This is a free open-source project, so the aim is "known state, bugs welcome" rather than a full test matrix:
please file what you find as [issues](https://github.com/ljw46/EmberlineSpectralis/issues) with your host, OS and version.

Legend: **P** tested and fine, **~** works with notes, **-** not tested.

## Coverage by area (v1.0.0, 2026-11-22)

| area | what was checked | Win: Ableton | Win: FL Studio | Mac: Ableton | Standalone (Win) | Linux |
|---|---|---|---|---|---|---|
| Install and scan | appears as an instrument | P | P | P | P | - |
| Hosting | GUI size and drawing, MIDI pitch, no stuck notes, save / reopen, host + MIDI automation, two instances, close / reopen GUI, remove from track | P | P | - | - | - |
| Audio | no crackle at low buffers | ~ | ~ | - | ~ | - |
| Sample rate | 44.1 / 48 / 96 kHz | - | - | - | - | - |
| Windows installer | installs, plugin found | P (Avast false positive, see below) | | | | |
| macOS installer | | | | - | | |
| Linux installer (.deb / tar.gz) | CI install + pluginval only | | | | | - |

Notes:

* **Crackle** at small buffers is a driver limit: ASIO is fine, DirectSound crackles at low buffer sizes.
* **Windows installer:** Avast flagged the unsigned setup.exe (suspected false positive) and the admin prompt is expected (it writes to
  `Program Files\Common Files\VST3`). Report the file to Avast; code signing is the real fix.
* **Not tested by a person:** Reaper, Linux hosts, the Mac installer (waiting until further changes settle), Logic (no AU), sample-rate
  changes, a 30-minute soak run.

## Sound and interface (Windows, 2026-11-22)

* **Fine:** default patch audible without clipping; all filter types and slopes work; Ladder at full resonance with fast cutoff moves stays
  stable; unison is in tune with no level jumps; preset changes are click-free; mod chips drop onto knobs and the route popup works;
  presets load; arp and stutter run.
* **Addressed for 1.0 (needs a listen):**
  * Drift was too subtle: its range is doubled. Drift and Warmth now show as whole percentages.
  * No preset set Drift or Warmth, so all used the defaults. Every preset now has its own (by category).
  * "Init - Basic Saw" is now a soft starting point (gentler filter, a little warmth).
  * Harsh presets rounded off: Glass Hymn, Lasers In The Fog, Neon Silk, Prism Lead, Spire, Glass Staircase, Rolling Gold, Electric Orchard.
    Neon Silk did not measure as bright, so check it by ear.
  * Rain On Leaves and Campfire no longer use full-strength crackle noise.
* **Known issues / ideas for 1.1:**
  * Stutter needs a proper speed / ramp **curve editor** (it is very uniform now), with those controls linkable to modulation.
  * Filters cannot be assigned per oscillator: all oscillators go through filter 1 and 2 (Serial or Parallel).
  * Macros cannot be renamed in the UI (names come from the preset file).
  * Many parameters are not valid mod-drop targets.
  * The window is a fixed size (not resizable).
  * A short tail is audible when changing preset with notes held (no clicks).
  * The "Crackle" noise type itself is still harsh (250 full-size clicks a second); only the presets were changed.

## Before a release tag

* [ ] CI green on Windows, macOS and Linux.
* [ ] Install the Windows and macOS installers once on a clean machine and open the plugin in one host.
* [ ] Open the plugin, play, save and reopen a project in at least one host per OS you can reach.
* [ ] Release notes say which platforms are untested.

## Results log

| date | build | OS / host | tester | notes / issues |
|---|---|---|---|---|
| 2026-11-22 | v1.0.0 (CI build of 6cc75f1, unsigned) | Windows: Ableton Live, FL Studio, Standalone; Mac: Ableton Live (scan only) | Liam | See tables above. |
