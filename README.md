# Emberline Spectralis

[![build](https://github.com/ljw46/EmberlineSpectralis/actions/workflows/build.yml/badge.svg)](https://github.com/ljw46/EmberlineSpectralis/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/ljw46/EmberlineSpectralis?include_prereleases&label=release)](https://github.com/ljw46/EmberlineSpectralis/releases/latest)
[![licence: AGPL-3.0](https://img.shields.io/badge/licence-AGPL--3.0-blue)](LICENSE)
![platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)
![formats](https://img.shields.io/badge/formats-VST3%20%7C%20Standalone-555)
[![Buy me a coffee](https://img.shields.io/badge/buy%20me%20a%20coffee-emberlinesky-ffdd00?logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/emberlinesky)

A wavetable synthesizer for melodic, deep and organic house. VST3 plugin and Standalone app, built with
[JUCE](https://juce.com) 8.

> **Status:** 1.0. Windows, macOS and Linux. Please report anything odd.

## Download

Grab the latest build from the [**Releases page**](https://github.com/ljw46/EmberlineSpectralis/releases/latest): download the zip for your
computer and run it:

| You have | Download |
|---|---|
| **Windows** | `EmberlineSpectralis-<version>-windows-setup.exe` (or the zip) |
| **Mac** (Apple Silicon or Intel) | `EmberlineSpectralis-<version>-macos.pkg` (or the zip) |
| **Linux** (x86_64) | `emberline-spectralis_<version>_amd64.deb`, or `EmberlineSpectralis-linux.tar.gz` for any distro |

The zips and tar.gz contain an `INSTALL.txt` with manual steps. It's free; please tell me what breaks.

**Code signing:** Windows builds are planned to be signed for free through the
[SignPath Foundation](https://signpath.org) (signing by [SignPath.io](https://signpath.io)) once the project qualifies.
Until then, downloads are unsigned and Windows may show a SmartScreen warning (More info, then Run anyway). See the
[code signing policy](CODE_SIGNING_POLICY.md).

## Features

* Three wavetable oscillators with mipmapped playback, warp modes, stereo unison, sub and noise
* Two filters per voice: lowpass, highpass, bandpass, notch, formant and ladder, with 12 / 24 dB slopes
* Analogue **Drift** and **Warmth** controls for a softer, less digital tone
* 16-slot modulation matrix with LFOs, envelopes, chaos and performance sources; drag a source onto a knob and
  set its range in place
* Per-oscillator wavetable view and a live filter-response display
* Poly / Mono / Legato voice modes with glide (portamento)
* FX chain, tempo sync and an optional **arpeggiator with stutter** (speeds up and slows down, built for
  melodic / deep-house rolls)
* 160 factory presets and 30+ factory wavetables, all plain text-editable JSON; import your own WAV
  single-cycle wavetables

## Build

Requires CMake 3.22+ and a C++20 compiler. JUCE is fetched automatically.

**Windows** (Visual Studio 2022 Build Tools):

    scripts\build-windows.bat

**Linux / macOS** (on Linux install `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev` first):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target EmberlineSpectralis_Standalone EmberlineSpectralis_VST3

The VST3 ends up in `build/EmberlineSpectralis_artefacts/Release/VST3/`; copy it to your VST3 folder
(Windows: `C:\Program Files\Common Files\VST3`, Linux: `~/.vst3`). Installers: see `installer/`.

`EmberlineTools` is a command-line helper (`check`, `render`, `selftest`, `snapshot`, `dump-params`) used for
development.

## Documentation

* [Presets format](docs/PRESETS.md) and [wavetables format](docs/WAVETABLES.md) - both written to be easy for
  people and LLMs to edit
* [User manual](docs/manual/README.md) - friendly guide for producers (also built into the plugin: click the **?** button)
* [Parameter list](docs/parameters.md)
* [Credits](docs/CREDITS.md)
* [Manual test checklist](docs/testing.md) and [performance notes](docs/performance.md)

## Licence

* **Source code:** [GNU AGPL-3.0](LICENSE).
* **Presets, wavetables and artwork:** [royalty-free EULA](LICENSE-CONTENT.md). Use them in your music freely;
  don't redistribute them.

Copyright (c) 2026 Liam Walsh.

<!-- Uncomment once SignPath approves the project and signing is live in CI (see CODE_SIGNING_POLICY.md):
Windows builds: free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org). See the [code signing policy](CODE_SIGNING_POLICY.md).
-->
