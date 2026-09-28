# GrooveTap Delay

An 8-band rhythmic / modulation delay plugin (VST3) for macOS, built with [JUCE](https://juce.com).

Each of the 8 delay lines is fully independent — its own time (≤696 ms), feedback,
low/high-cut loop filter, pan, level, and its own LFO (rate, depth, and a
selectable single or dual waveform) for chorus/flange/pitch-mod style
modulation. Lines can be routed in parallel or chained in series for up to
~5.9 seconds of combined delay. Includes a 999-slot patch bank (90 read-only
factory patches + 90 user patches that start as editable copies of them,
plus 819 extra free slots), tap tempo, and MIDI-clock note-division sync
per band.

## ⚠️ Disclaimer / provenance

This project is an **independent, unofficial, fan-made** effect plugin. It is
**not affiliated with, endorsed by, or produced in association with Yamaha
Corporation or Allan Holdsworth's estate.**

Its feature set (8 independent delay lines, per-band modulation, tap-tempo,
patch banking) was **functionally inspired by** the Yamaha UD-Stomp hardware
pedal. The DSP, UI, and all code in this repository are original
clean-room implementations — none of it is decompiled or copied from
Yamaha's firmware or software.

The 90 **factory preset parameter values** bundled in
[`Source/FactoryPresets.cpp`](Source/FactoryPresets.cpp) were extracted
programmatically from the numeric patch-list tables printed in the UD-Stomp
owner's manual (a copyrighted document). Reproducing a manufacturer's
factory preset library and a named musician's signature patch data is a
legal gray area beyond simple "facts aren't copyrightable" territory, which
is why **this repository is kept private** rather than published publicly.
Do not make this repository (or the preset data specifically) public
without your own legal assessment. If you want a safer public release,
strip or replace `Source/FactoryPresets.cpp` with your own original patch
data first.

"Yamaha" and "UD-Stomp" are trademarks of Yamaha Corporation, referenced
here only to describe the hardware that functionally inspired this plugin.

## Features

- **8 independent delay lines**, each with:
  - Time (0–696 ms), Feedback, Low Cut / High Cut loop filter
  - Pan, Level
  - Its own LFO: Speed, Depth, and Wave (Sine / Triangle / Saw Up / Saw Down,
    or dual-shape combinations like Sine + Triangle)
  - **Tap %**: the real UD-Stomp "TAP" control — the delay is actually taken
    from the loop at this percentage of the Time setting
  - **Clock**: optional MIDI/host-tempo note-division sync (1/16 through
    2 Bar), overriding the manual Time knob for that band
- **Topology**: Parallel or Series (series chains all 8 bands for up to
  ~5.9 s of combined delay)
- **Global**: Effect Level, Direct Level, Direct Pan, Output, Bypass, Tap Tempo
- **999-slot patch bank**: 90 read-only Factory patches (real names/values
  transcribed from the manual), 90 User patches (start as editable copies of
  the Factory bank), and 819 extra free User slots. Prev/Next, a
  type-a-number jump box, a Browse menu, rename, and Save.
- **UI** mirrors the hardware's actual panel layout: an EFFECT BAND 1–8
  select strip, an EDIT FUNCTION cluster (GROUP / SYNC / WAVE), and the two
  knob rows (TAP/SPEED/DEPTH/PAN/LEVEL and TIME/LOW CUT/HIGH CUT/FEEDBACK).
  Fully resizable window.

## Known limitations

Things the real hardware has that this plugin intentionally does not
attempt to replicate, because they have no meaningful DAW-plugin equivalent
or would require guessing undocumented behavior:

- No MIDI I/O, no S/PDIF digital output, no physical footswitches.
- No phrase/loop-sampling function.
- Only two global routing topologies (all-Parallel or all-Series) — the
  hardware's arbitrary per-band CONNECT/GROUP sub-groupings (e.g. bands 1–3
  in series feeding bands 4–8 in parallel) are not implemented.
- The internal curve mapping Speed/Depth knob values (0–10, confirmed from
  the manual) to actual Hz/ms is an inferred exponential/power-law curve,
  tuned by ear — the manual does not document the hardware's exact internal
  curve, so this is a best-effort match, not a byte-exact reproduction.
- Two parsed manual columns (per-band "Connect Group" phase/series-linking
  code, and a phase-invert toggle) are read but not applied — all factory
  presets load as independent parallel bands regardless of what the manual's
  CONNECT column says.

## Building (macOS, Apple Silicon or Intel)

Requirements:
- Xcode command-line tools (`xcode-select --install`)
- CMake 3.22+ (`brew install cmake`)
- [JUCE](https://github.com/juce-framework/JUCE) checked out as a sibling
  directory: `~/dev/JUCE` (or pass `-DJUCE_DIR=/path/to/JUCE` — see below)

```bash
git clone --depth 1 https://github.com/juce-framework/JUCE.git ~/dev/JUCE
git clone <this-repo-url>
cd GrooveTapDelay
cmake -B build
cmake --build build --config Release -j$(sysctl -n hw.ncpu)
```

The build automatically copies the built `.vst3` into
`~/Library/Audio/Plug-Ins/VST3/`. Restart your DAW (or rescan plugins) to
pick it up.

To use a JUCE checkout at a different path, edit the `add_subdirectory`
line at the top of `CMakeLists.txt`.

## Project layout

```
Source/
  PluginProcessor.h/.cpp   DSP, parameters, patch-bank logic
  PluginEditor.h/.cpp      UI (hardware-inspired layout)
  FactoryPresets.h/.cpp    The 90 factory patches (see disclaimer above)
CMakeLists.txt
```

## License

The original code in this repository (everything except the factory preset
*values* transcribed from Yamaha's manual, see disclaimer above) is
provided as-is for personal/educational use. No warranty of any kind.
