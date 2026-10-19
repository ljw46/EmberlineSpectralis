# Presets, saving and sharing

## Browsing

The preset bar is at the top left: **[ < ] [ name ] [ > ] [ SAVE ]**.

- **< and >** step through every preset one by one.
- **Click the name** to open a menu with all the presets sorted by type: Bass, Pluck, Lead, Pad, Keys, Chord, Arp, Organic,
  Texture, FX and your own (USER).

Every preset starts from a clean slate, so a preset never picks up leftovers from the sound before it.

## Who made it

Presets can carry a name and a link, shown top right as **by <artist>** with the link underneath. Click it to open it in
your browser. For more detail, open the preset menu and choose **Preset info...** to see the author, link, description and tags.
The link only ever opens normal web addresses.

## Saving your own

1. Make a sound you like.
2. Click **SAVE**.
3. Fill in the box:
   - **Name** - what you'll call it.
   - **Category** - where it lives in the menu (USER by default).
   - **Description** - a line or two on what it sounds like or when to use it.
   - **Tags** - a few words, separated by commas (e.g. warm, pad, melodic).
   - **Author / artist name** and **Link** - your name and your Instagram, website or anywhere you want people to find you.
     The synth remembers these for next time, so you only type them once.
4. Click **Save**.

Your preset is saved as a plain text file in your user folder, and shows up in the menu straight away:

- Windows: `%APPDATA%\Emberline\Spectralis\Presets\`
- Mac: `~/Library/Application Support/Emberline/Spectralis/Presets/`
- Linux: `~/.config/Emberline/Spectralis/Presets/`

The menu has **Show user preset folder** to jump straight there, and **Rescan user presets** if you've been adding files by hand.

## Sharing a preset

Open the preset menu:

- **Export current sound as .json...** - saves the sound you're hearing to a file wherever you like, ready to send to
  a mate. It keeps the author, link and description of the preset you started from (or yours, if you've saved it).
  If your sound uses one of your own wavetables, you'll get a reminder: send that file as well, because it isn't
  stored inside the preset.
- **Import preset file(s)...** - pick one or more `.json` files someone sent you. They land in your user folder and
  appear in the menu. Nothing gets overwritten, so if the name already exists it just gets a number added.

You can also drop a `.json` file straight into the presets folder and hit **Rescan user presets**.

## Making presets for the library

Want to make presets for the factory library? Brilliant. See [CONTRIBUTING](../../CONTRIBUTING.md). In short: make it,
credit yourself in the save box, export it and send it over.

## Editing the files by hand

Presets are readable text (JSON), so you can open one in any text editor and change a number if you fancy. The format is
explained in [docs/PRESETS.md](../PRESETS.md). If you break a file, it just gets ignored. It won't crash anything.
