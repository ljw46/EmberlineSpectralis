# Performance

Measure with a **Release** build:

    build/EmberlineTools_artefacts/Release/EmberlineTools perf

It renders held chords at 48 kHz / 256-sample blocks on one core (a fresh instance per scenario) and prints CPU as a percentage of
real time, so 100 % is the point where a DAW starts to drop out. The plugin has 8 voices and is single-threaded.

Reference run (WSL2 on the dev machine, 2026-10-19, gcc Release with LTO; other machines will differ, so compare
before / after on the same machine):

| scenario | CPU |
|---|---|
| default patch, 1 voice | 0.5 % |
| default patch, 8 voices | 4.1 % |
| all four FX on, default patch, 1 voice | 2.5 % |
| 3 osc x 8 unison, 24 dB ladder + filter, warmth, no FX, 1 voice / 8 voices | 1.9 % / 15.0 % |
| same with all four FX, 8 voices | 17.0 % |
| **worst case**: 3 osc x 16 unison + warp, ladder + 24 dB filter, warmth, all FX, 8 voices | 29.3 % |
| arp 1/32, 2 osc x 8 unison, ladder, delay + reverb, 8-note chord | 7.9 % |
| factory presets, 8-note chord: median / heaviest | 6.8 % / 11.6 % (Solar Flare) |

So the heaviest thing the UI allows costs about 3x less than the limit, and the heaviest factory preset uses about a tenth of one core.
Each fully-loaded voice (24 unison oscillators, two 24 dB filters) costs roughly 2 %; the FX chain is a fixed ~2.5 %.
Not covered: tiny host buffers (32-64 samples, where per-block overhead matters more), the GUI thread, and Windows / macOS compilers.
