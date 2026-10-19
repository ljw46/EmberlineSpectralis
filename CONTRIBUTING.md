# Contributing presets

Presets are plain JSON files, so anyone can make one. Format reference: [docs/PRESETS.md](docs/PRESETS.md).

1. Design the sound in Emberline Spectralis.
2. Click **SAVE** in the preset bar. Fill in the name, category, description, tags, your **author name** and an
   optional **link** (website or socials). Your name and link are remembered for next time.
3. Find the file with the preset menu > **Show user preset folder**, or use **Export current sound as .json...**
   to write it anywhere you like.
4. Send it in:
   * open a pull request adding it to `Presets/Factory/`, or
   * attach the `.json` to an issue or email if you'd rather not use git.

Guidelines:

* Give it a clear `description` (what it sounds like, when to use it), 3-6 `tags` and a `category`.
* Set the four `macro_names` if the macros do something useful.
* Stick to factory wavetables. If you need your own, send the wavetable file as well.
* Keep levels sensible: no clipping, and roughly the same loudness as the other presets.
* Use the **Preset info...** menu item to check that your credit and link show up correctly.

By submitting a preset you confirm it is your own work and agree that it may be distributed with Emberline
Spectralis under the [content licence](LICENSE-CONTENT.md). You are credited as the author, and you keep the
right to use your own sounds however you like. Contributions to the source code are under AGPL-3.0.
