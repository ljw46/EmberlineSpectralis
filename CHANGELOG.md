# Changelog

Versions follow `MAJOR.MINOR.PATCH`. The number lives in one place: `project(... VERSION x.y.z)` in `CMakeLists.txt`
(the release stage label is `EMBER_VERSION_LABEL`). Release tags look like `v1.0.0` (or `v1.1.0-beta.1` for a pre-release).

## 1.0.0

* **First stable release.** Windows, macOS and now **Linux** (VST3 and Standalone, x86_64).
* **Installers:** a Windows setup `.exe`, a macOS `.pkg` and a Linux `.deb` plus a no-root `tar.gz`. Plain zips are still available.
* **Drift is stronger** (about twice the pitch wander at the same setting) and **Drift / Warmth now show as percentages.**
* **Every preset has its own Drift and Warmth** (they used to all share the defaults): pads and textures wander more, plucks and keys get more warmth, basses stay steadier.
* Softer **Init** preset; eight harsh presets (Glass Hymn, Lasers In The Fog, Neon Silk, Prism Lead, Spire, Glass Staircase, Rolling Gold, Electric Orchard) have a rounder top end.
* **Rain On Leaves** is now a soft shower instead of static-like crackle; **Campfire** has gentler crackle.
* Everything from the betas, below.


### Added during the betas

* Analogue **Drift** and **Warmth** controls.
* Filters: 12 / 24 dB slope, plus Notch and Ladder types.
* Per-oscillator wavetable view; live filter-response display.
* Modulation route popup when dropping a source on a knob (right-click a knob to reopen it).
* **Arpeggiator and stutter** on the new ARP & STUTTER tab, with 7 factory presets that use it.
* **Mono and Legato** voice modes and **Glide** (portamento), plus 4 presets: Tidal Slide and Moon Drop (bass), Sunrise Slide (lead), Unalived Rat (pluck).
* The manual is built into the plugin: the **?** button next to the preset bar opens it in your browser (offline). The ? menu and a small header line also link to buy-me-a-coffee.
* Preset author and link, shown in the header (click to open); export / import of preset files.
* 160 factory presets, 30+ factory wavetables, WAV wavetable import.
* Version shown in the UI; presets and saved sessions record their version / schema.
