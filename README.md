# Mix Mentor (v0.1, basic prototype)

A plugin that teaches you to mix while you mix. Put it on a track, pick your genre and the
instrument, and it shows you:

- **A spectrum analyzer with genre-specific frequency zones.** Green = usually worth boosting,
  red = usually worth cutting, yellow = watch out. **Hover over a zone** for a tip.
- **Tip cards** for EQ, Compression, Sidechain, Reverb, Delay and Gain Staging, written for
  that exact genre + instrument.
- **A gain staging meter** with the target peak range for that instrument and genre, and a plain-English
  verdict ("Too hot, turn it down 3 dB", "Good level").

The audio passes through untouched: it only listens. Put it **last** on the track (after your
EQ/compressor) to see the result of your processing.

On first load it asks what genre you're making. If your DAW tells plugins the track name
(e.g. a track called "Kick" or "Lead Vox"), it guesses the instrument automatically.

**Included in v0.1:** Hip-Hop/Trap, Pop, House/EDM × Kick, Snare/Clap, Bass/808, Lead Vocal.

---

## Getting the plugin

### Option A: let GitHub build it (no tools to install)
1. Create a GitHub repo and upload this folder.
2. Open the repo's **Actions** tab. The "Build plugin" workflow runs automatically (~10-20 min).
3. Open the finished run and download **MixMentor-Windows** or **MixMentor-macOS** under *Artifacts*.

### Option B: build it on your computer
You need **CMake 3.22+** and a C++ compiler:
- **Windows:** Visual Studio 2022 (Community is free) with "Desktop development with C++".
- **Mac:** Xcode (free from the App Store).

Then, in this folder:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE (the plugin framework) downloads automatically the first time. The plugin ends up in
`build/MixMentor_artefacts/Release/`.

### Installing
| | Copy `Mix Mentor.vst3` to | AU (Mac only, `Mix Mentor.component`) |
|---|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` | n/a |
| Mac | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |

Then rescan plugins in your DAW. Works in Studio One, FL Studio, Ableton Live, Cubase,
Reaper, Bitwig and Logic (AU).

**Mac note:** the plugin isn't code-signed yet, so macOS may block it. Run this once in Terminal:
`xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Mix Mentor.vst3"` (and the same for the `.component`).

There's also a **Standalone** app build for quick testing outside a DAW.

---

## Adding or changing advice

All the knowledge lives in **`Resources/knowledge.json`**. No C++ needed:

- **New genre:** copy a genre block, change `id`, `name`, `summary` and the advice.
- **New instrument:** add it to `instruments` (with track-name `keywords`), then add a guide
  for it inside each genre's `guides`.
- **Zones:** `lo`/`hi` in Hz, `kind` = `boost` | `cut` | `watch`, `label`, and `tip` (the hover text).
- **`peakRange`:** the target peak level in dBFS for the gain staging meter.

Rebuild after editing (the file is compiled into the plugin).

## Code layout
| File | What it does |
|---|---|
| `Source/KnowledgeBase.*` | Loads and looks up `knowledge.json` |
| `Source/PluginProcessor.*` | Audio pass-through, level measurement, feeds the analyzer, saves genre/instrument with your project, track-name guessing |
| `Source/PluginEditor.*` | The window: genre picker, dropdowns, tabs, tips, gain staging text |
| `Source/SpectrumView.*` | FFT analyzer, zone overlay, hover tooltips |
| `Source/LevelMeter.*` | Peak/RMS meter with target range |

## Ideas for next versions
- More genres (Rock, Techno, R&B, Drum & Bass, Lo-fi) and instruments (hi-hats, guitars, synths,
  pads, backing vocals, **mix bus and mastering**).
- "Problem detector": flag build-ups automatically (e.g. too much energy in the mud zone compared
  with the genre's reference).
- Reference curves: overlay a typical genre spectrum for that instrument.
- Step-by-step guided workflow ("1. Gain stage → 2. Subtractive EQ → 3. Compression...").


## v0.9

**Mix analysis**
- The fix list now checks more than EQ:
  - clipping and peak ceiling
  - punch vs. squash (crest factor)
  - uneven level (compressor settings per instrument)
  - loudness against genre targets (master)
  - stereo low end, phase and centre width
- Each item gives concrete settings: EQ bell frequency/Q/gain and compressor ratio/attack/release/gain reduction.
- Fixed items stay fixed when you analyse again. They only come back if they clearly got worse.
- Tracks compare notes: put Mix Mentor on several tracks and analyse each. It warns about kick/bass, bass/chords and vocal/instrument clashes.
- The list scrolls; click an item to see how to fix it.

**Melody**
- Key + scale picker (auto, or lock to your song's key; 8 scales). Every note of every idea is in key.
- Ideas are written by a note model trained on the melodies of the POP909 dataset (909 songs, MIT licence) and scored with classic melody rules: resolve leaps, stable notes on strong beats, one clear high point, end on a stable note.
- New idea: "fresh rhythm", built from the most common one-bar rhythms in real songs, repeating A A B A.
- Training scripts: train/train.py, train/gen_header.py.
