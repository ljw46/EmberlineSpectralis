#!/usr/bin/env python3
"""Builds the sampled factory wavetables from the Adventure Kid Waveforms (AKWF) collection.

AKWF (c) Kristoffer Ekstrand / Adventure Kid, released under CC0 1.0 (public domain):
https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE

  python3 tools/akwf/fetch.py      # downloads the chosen families into ./src   (run it in a scratch folder)
  python3 tools/akwf/build.py src  # writes Wavetables/Factory/Audio/*.wav + a JSON recipe per table

For each family the single cycles are ordered by spectral brightness (centroid), 16 evenly spaced ones are picked,
each is DC-removed, peak-normalised and resampled to 2048 samples, and the 16 cycles are written back to back as one
WAV (the Serum / Vital layout). Frame therefore sweeps from the darkest to the brightest recording.
"""
import glob, json, math, os, struct, sys, wave

SRC = sys.argv[1] if len(sys.argv) > 1 else "src"
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
OUT = os.path.join(ROOT, "Wavetables", "Factory")
AUDIO = os.path.join(OUT, "Audio")
N = 2048

# family -> (table name, category, description, tags)
TABLES = {
    "hvoice":   ("Human Voice", "Vocal", "Recorded human-voice cycles ordered dark to bright: Frame moves from rounded, 'oo'-like colours toward open, bright vowels.", ["voice", "vowel", "choir", "recorded"]),
    "flute":    ("Flute", "Organic", "Recorded flute cycles, dark and breathy to bright and edgy. Natural, airy and slightly imperfect.", ["flute", "wind", "recorded"]),
    "clarinett":("Clarinet", "Organic", "Recorded clarinet cycles: hollow, odd-harmonic woodwind tones from mellow to reedy.", ["clarinet", "reed", "recorded"]),
    "oboe":     ("Oboe", "Organic", "Recorded oboe cycles: nasal, vocal reed tones, soft to piercing.", ["oboe", "reed", "recorded"]),
    "cello":    ("Cello", "Organic", "Recorded cello cycles from a dark, woody bow to a bright, rasping one.", ["cello", "strings", "bowed", "recorded"]),
    "violin":   ("Violin", "Organic", "Recorded violin cycles, soft and sweet through to bright and scratchy.", ["violin", "strings", "bowed", "recorded"]),
    "altosax":  ("Alto Sax", "Organic", "Recorded alto-saxophone cycles: warm and breathy to bright and buzzy.", ["sax", "reed", "recorded"]),
    "aguitar":  ("Acoustic Guitar", "Organic", "Recorded acoustic-guitar cycles, from a soft body to a bright pick attack.", ["guitar", "plucked", "recorded"]),
    "eguitar":  ("Electric Guitar", "Organic", "Recorded electric-guitar cycles: clean to bitey, with the pickup's character.", ["guitar", "electric", "recorded"]),
    "theremin": ("Theremin", "Organic", "Recorded theremin cycles: pure, eerie sine-like tones to richer ones.", ["theremin", "eerie", "recorded"]),
    "birds":    ("Bird Song", "Organic", "Bird-call cycles: whistly, chirpy and strange. Use sparingly for organic sparkle.", ["birds", "nature", "recorded"]),
    "piano":    ("Grand Piano", "Keys", "Recorded piano cycles from a soft, round tone to a bright, struck one.", ["piano", "keys", "recorded"]),
    "epiano":   ("Electric Piano", "Keys", "Recorded electric-piano cycles (Rhodes / Wurlitzer family): warm bell tones to bright bark.", ["epiano", "rhodes", "keys", "recorded"]),
    "eorgan":   ("Tonewheel Organ", "Keys", "Recorded electric-organ cycles: pure flute registrations through to full, bright drawbar sounds.", ["organ", "keys", "recorded"]),
    "clavinet": ("Clavinet", "Keys", "Recorded clavinet cycles: funky, twangy and percussive, soft to biting.", ["clavinet", "funk", "keys", "recorded"]),
    "ebass":    ("Electric Bass", "Bass", "Recorded electric-bass cycles: round and deep to bright and growling.", ["bass", "electric", "recorded"]),
    "dbass":    ("Digital Bass", "Bass", "Digital bass cycles: clean subs to gritty, harmonically rich bass waves.", ["bass", "digital", "recorded"]),
    "bw_blended":("Blended Waves", "Analog", "Classic synth waves blended into one another: saws, squares and in-betweens, ordered dark to bright.", ["analog", "blended", "classic"]),
    "overtone": ("Overtone Series", "Analog", "Cycles built from overtone stacks: bell- and organ-like partial structures, soft to bright.", ["overtone", "additive", "hollow"]),
    "hdrawn":   ("Hand Drawn", "Analog", "Hand-drawn waveforms: irregular, unmistakably human shapes with unusual harmonics.", ["hand drawn", "odd", "character"]),
    "sinharm":  ("Sine Harmonics", "Analog", "Sines with added harmonics: simple, clean timbres that build up in richness.", ["sine", "harmonics", "clean"]),
    "c604":     ("C604 Chip", "Analog", "Waves from a hardware sampler's oscillator bank: tight, characterful classic-synth shapes.", ["hardware", "classic", "synth"]),
    "fmsynth":  ("FM Synth Bank", "FM", "Recorded FM-synthesizer cycles: glassy, metallic and electric-piano-like, dark to bright.", ["fm", "digital", "glassy", "recorded"]),
    "oscchip":  ("Chip Oscillators", "Digital", "Waves from classic oscillator chips: gritty, lo-fi and full of aliasing character.", ["chip", "lofi", "digital"]),
    "vgame":    ("Video Game", "Digital", "Video-game console waves: pulse, noise-like and quirky arcade tones.", ["chiptune", "arcade", "retro"]),
    "distorted":("Distorted", "Digital", "Overdriven and folded waveforms: aggressive, buzzy and harmonically dense.", ["distorted", "harsh", "buzzy"]),
    "granular": ("Granular Grit", "Digital", "Granular-textured cycles: rough, crackly and unstable. Good for movement and noise.", ["granular", "texture", "gritty"]),
    "snippets": ("Snippets", "Digital", "Tiny cuts from larger recordings: unpredictable, instrument-ish and strange.", ["snippets", "odd", "texture"]),
    "raw":      ("Raw Cuts", "Digital", "Raw, unprocessed cycles from assorted sources: rough around the edges.", ["raw", "lofi", "texture"]),
    "linear":   ("Linear Ramps", "Digital", "Piecewise-linear shapes: ramps, steps and corners with a clean, geometric sound.", ["linear", "geometric", "clean"]),
    "pluckalgo":("Pluck Algorithm", "Digital", "Waves produced by plucked-string algorithms: sharp, string-like attacks.", ["pluck", "string", "digital"]),
}

def read_cycle(path):
    w = wave.open(path)
    data = struct.unpack("<%dh" % w.getnframes(), w.readframes(w.getnframes()))
    mean = sum(data) / len(data)
    x = [v - mean for v in data]
    peak = max(abs(v) for v in x) or 1.0
    return [v / peak for v in x]

def centroid(x):
    n = len(x)
    cos = [math.cos(2 * math.pi * k / n) for k in range(n)]
    sin = [math.sin(2 * math.pi * k / n) for k in range(n)]
    num = den = 0.0
    for h in range(1, 61):
        re = sum(x[i] * cos[(i * h) % n] for i in range(n))
        im = sum(x[i] * sin[(i * h) % n] for i in range(n))
        a = math.hypot(re, im)
        num += h * a
        den += a
    return num / den if den else 0.0

def resample(x, n=N):
    m = len(x)
    out = []
    for i in range(n):
        p = i * m / n
        k = int(p); f = p - k
        y0, y1, y2, y3 = x[(k - 1) % m], x[k % m], x[(k + 1) % m], x[(k + 2) % m]
        c1 = 0.5 * (y2 - y0); c2 = y0 - 2.5 * y1 + 2 * y2 - 0.5 * y3; c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2)
        out.append(((c3 * f + c2) * f + c1) * f + y1)
    peak = max(abs(v) for v in out) or 1.0
    return [v / peak for v in out]

def write_wav(path, frames):
    samples = [v for fr in frames for v in fr]
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100)
        w.writeframes(struct.pack("<%dh" % len(samples), *[int(max(-1, min(1, v)) * 32767) for v in samples]))

def main():
    os.makedirs(AUDIO, exist_ok=True)
    made = []
    for family, (name, cat, desc, tags) in TABLES.items():
        files = sorted(glob.glob(os.path.join(SRC, family, "*.wav")))
        if len(files) < 2:
            print("skip", family, "(not enough files)"); continue
        cycles = [read_cycle(f) for f in files]
        order = sorted(range(len(cycles)), key=lambda i: centroid(cycles[i]))
        picks = order if len(order) <= 16 else [order[round(k * (len(order) - 1) / 15)] for k in range(16)]
        write_wav(os.path.join(AUDIO, name + ".wav"), [resample(cycles[i]) for i in picks])
        recipe = {"name": name, "category": cat, "description": desc, "tags": tags, "mode": "wav", "file": name + ".wav",
                  "frame_size": N, "source": f"AKWF_{family} (Adventure Kid Waveforms, CC0)"}
        with open(os.path.join(OUT, name + ".json"), "w") as f:
            json.dump(recipe, f, indent=4, ensure_ascii=False); f.write("\n")
        made.append((cat, name, len(picks), len(files)))
        print(f"{name:20s} {cat:8s} {len(picks):2d} frames from {len(files)} cycles")
    print(len(made), "tables")

main()
