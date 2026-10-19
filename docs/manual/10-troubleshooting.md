# Troubleshooting

**My DAW doesn't see the plugin.**
Check that `Emberline Spectralis.vst3` is in your VST3 folder (Windows: `C:\Program Files\Common Files\VST3`) and rescan plugins.
Some DAWs only rescan if you tell them to.

**Mac says it can't be opened / is from an unidentified developer.**
The beta isn't signed yet. Right-click the plugin, choose **Open**, or in Terminal:
`xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Emberline Spectralis.vst3"` (change the path if you put it elsewhere).

**Windows shows a SmartScreen warning.**
Same reason. Click **More info**, then **Run anyway**.

**No sound.**
Make sure the track is getting MIDI and is armed. Check that **OSC 1 is switched on** and its **LEVEL** is up, and that the **VOLUME**
knob in the Master panel isn't right down. If you've got the arp on, you need to **hold** a chord (or turn on LATCH).

**Crackling or dropouts.**
Raise your DAW's audio buffer size (256 or 512 is fine for jamming). Lower the **UNISON** on the oscillators and switch off any
effects you're not using. Lots of unison on all three oscillators is the heaviest thing it does.

**I only hear one note when I play a chord.**
The Voice mode is on **Mono** or **Legato** (Filters & Envs tab, VOICE panel). Set it back to **Poly** for chords.

**Notes slide into each other and I don't want that.**
Turn **GLIDE** down to 0, or set glide mode to **Legato Only** so it only slides when notes overlap.

**It sounds too dull / too clean / too harsh.**
That's the **WARMTH** and **DRIFT** knobs in the Master panel (more warmth = softer and darker, 0 = clean). If it's still harsh, try the
**Ladder** filter, or pull down the filter **CUTOFF**.

**The arp isn't in time with my song.**
It follows your DAW's tempo and transport when it's playing. If you jump around the timeline it re-locks to the beat. In the Standalone
app there's no DAW, so it uses the **TEMPO** knob in the Filters tab.

**The arp keeps playing after I let go.**
**LATCH** is on. Turn it off (or play new notes to replace the held chord).

**A preset I downloaded loads but sounds different.**
It may use one of the author's own wavetables. Ask them to send the wavetable file as well, and see [Your own wavetables](08-wavetables.md).

**My saved preset isn't in the menu.**
Open the preset menu and choose **Rescan user presets**. (For wavetables it's **Rescan wavetable folder** in the oscillator's table list.) If you saved it by hand, check that the file ends in `.json` and is valid JSON.
(A broken file is skipped quietly.)

**Where are my files?**
- Windows: `%APPDATA%\Emberline\Spectralis\` (Presets and Wavetables folders)
- Mac: `~/Library/Application Support/Emberline/Spectralis/`
- Linux: `~/.config/Emberline/Spectralis/`

**Something's properly broken.**
Tell us! Open an issue on the project page with your DAW, operating system, the plugin version (under the preset bar)
and what you did.
