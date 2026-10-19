# Your own wavetables

You can feed the synth your own sounds. Anything you load turns up in the table list of every oscillator as
**User 1** to **User 8**.

## The easy way: import from the table list

1. Click the **table name** on any oscillator to open the list.
2. At the bottom choose **Import wavetable (.wav / .json)...** and pick your file(s). They're copied into your user folder
   for you, and the new table gets selected straight away.
3. The list also has **Rescan wavetable folder** and **Show wavetable folder**.

Your files fill the **User 1-8** slots in alphabetical order.

## Or drop the files in yourself

1. Put a `.wav` file in your user wavetable folder:
   - Windows: `%APPDATA%\Emberline\Spectralis\Wavetables\`
   - Mac: `~/Library/Application Support/Emberline/Spectralis/Wavetables/`
   - Linux: `~/.config/Emberline/Spectralis/Wavetables/`
2. In any oscillator's table list, choose **Rescan wavetable folder** (or just restart).
3. Pick **User 1** or whichever slot your file landed in.

### What kind of WAV works?

- **A "wavetable" WAV** (the kind Serum and Vital use): a file made of several frames of **2048 samples** each. It gets
  squeezed into 16 frames and you sweep through them with **FRAME**.
- **A single cycle** of any length (for example from AKWF packs): used as one waveform.
- Stereo files are mixed to mono. 

The first 8 files (alphabetical) get the slots. Rename them with numbers if you want to control the order.

## Wavetables as text

Wavetables can also be written as text (`.json`): you describe harmonics or shapes and the synth builds the table.
That's more of a nerd thing, and it's explained in [docs/WAVETABLES.md](../WAVETABLES.md). You can also paste
that guide into an AI chat to get it to write wavetables for you.

## Sharing

A preset that uses your own wavetable only works for someone else if they've got the same file. Send both!
Make sure you've got the rights to share any samples you didn't make yourself.
