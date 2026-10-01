# MOKU AI UD Stomp

*(previously "GrooveTap Delay" — renamed; see the trademark note below before
you make this repo public or distribute it.)*

An 8-band rhythmic / modulation delay plugin (VST3) for macOS, built with [JUCE](https://juce.com).

Each of the 8 delay lines is fully independent — its own time (≤696 ms), feedback,
low/high-cut loop filter, pan, level, and its own LFO (rate, depth, and a
selectable single or dual waveform) for chorus/flange/pitch-mod style
modulation. Lines can be routed in parallel or chained in series for up to
~5.9 seconds of combined delay. Includes a 999-slot patch bank (90 read-only
factory patches + 90 user patches that start as editable copies of them,
plus 819 extra free slots), tap tempo, MIDI-clock note-division sync per
band, MIDI-triggerable footswitch actions, and in-window keyboard shortcuts.

## ⚠️ Disclaimer / provenance — read before publishing or distributing

This project is an **independent, unofficial, fan-made** effect plugin. It is
**not affiliated with, endorsed by, or produced in association with Yamaha
Corporation or Allan Holdsworth's estate.**

**Trademark note on the current name.** The product was renamed from
"GrooveTap Delay" to **"MOKU AI UD Stomp"**, which now contains "UD Stomp"
verbatim — Yamaha's actual product name. The original name was chosen
specifically to avoid that overlap. Using a manufacturer's product name as
part of your own product's name (not just as a factual "inspired by"
reference) is a materially higher trademark risk than the previous name,
independent of the factory-preset-data risk noted below. This repository
stays **private** for that reason too. If you plan to distribute this
build (even informally), rename it again first, or get clearance.

Its feature set (8 independent delay lines, per-band modulation, tap-tempo,
patch banking) was **functionally inspired by** the Yamaha UD-Stomp hardware
pedal. The DSP, UI, and all code in this repository are original
clean-room implementations — none of it is decompiled or copied from
Yamaha's firmware or software. The faceplate/knob/button artwork was
separately AI-generated for this project (not scanned or copied from the
real hardware or its manual) — see `Resources/`.

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
- **Assign**: a free macro knob that drives whichever global control (Output /
  Effect Level / Direct Level / Direct Pan) the target selector points at —
  a live pass-through control, not a persisted DSP parameter, mirroring the
  hardware's own assignable-control knob
- **999-slot patch bank**: 90 read-only Factory patches (real names/values
  transcribed from the manual), 90 User patches (start as editable copies of
  the Factory bank), and 819 extra free User slots. Prev/Next, a
  type-a-number jump box, a Browse menu, rename, and Save.
- **MIDI**: any knob is host-MIDI-mappable for free (e.g. Ableton's own
  Cmd+M MIDI Map mode — no plugin code needed for that). Additionally the
  plugin itself listens for 4 fixed MIDI notes mirroring the hardware's
  footswitches: Select Band (cycle, default note 36/C1), Band On/Off toggle
  (37/C#1), Tap Tempo (38/D1), Bypass (39/D#1). Not yet remappable via a
  MIDI-learn UI.
- **Keyboard shortcuts** (plugin window focused): `1`–`8` select band, `[`
  / `]` or arrow keys step patches, `T` taps tempo, `B` toggles bypass.
- **UI**: image-based faceplate, knobs, and footswitches (AI-generated
  assets, see disclaimer) mirroring the hardware's actual panel layout — an
  EFFECT BAND 1–8 select strip, a per-band ON/OFF row, an EDIT SETTINGS
  cluster (GROUP / SYNC / WAVE), and the two 6-knob rows plus a separate
  Output/Assign strip. Fully resizable window.

## Known limitations

Things the real hardware has that this plugin intentionally does not
attempt to replicate, because they have no meaningful DAW-plugin equivalent
or would require guessing undocumented behavior:

- No S/PDIF digital output, no AD/DA converter emulation (see below).
- No phrase/loop-sampling function.
- Only two global routing topologies (all-Parallel or all-Series) — the
  hardware's arbitrary per-band CONNECT/GROUP sub-groupings (e.g. bands 1–3
  in series feeding bands 4–8 in parallel), inter-band modulation phase
  sync, and per-band polarity (REV) invert are not implemented yet. Note:
  our own "SYNC/Clock" feature is a different, original addition (host-tempo
  note-division sync) and is **not** the same as the hardware's real SYNC
  function (which links one band's LFO phase to another's) — don't confuse
  the two.
- The internal curve mapping Speed/Depth knob values (0–10, confirmed from
  the manual) to actual Hz/ms is an inferred exponential/power-law curve,
  tuned by ear — the manual does not document the hardware's exact internal
  curve, so this is a best-effort match, not a byte-exact reproduction.
- AD/DA converter modeling (the manual's 44.1kHz / 20-bit+3-bit-floating
  A/D spec) is deliberately not attempted — speculative without reference
  hardware to measure against, and low value in an all-digital DAW chain.

## Building (macOS, Apple Silicon or Intel)

Requirements:
- Xcode command-line tools (`xcode-select --install`)
- CMake 3.22+ (`brew install cmake`)
- [JUCE](https://github.com/juce-framework/JUCE) checked out as a sibling
  directory: `~/dev/JUCE` (or edit the `add_subdirectory` path in
  `CMakeLists.txt`)

```bash
git clone --depth 1 https://github.com/juce-framework/JUCE.git ~/dev/JUCE
git clone <this-repo-url>
cd MokuAIUDStomp
cmake -B build
cmake --build build --config Release -j$(sysctl -n hw.ncpu)
```

The build automatically copies the built `.vst3` into
`~/Library/Audio/Plug-Ins/VST3/`. Restart your DAW (or rescan plugins) to
pick it up.

## Project layout

```
Source/
  PluginProcessor.h/.cpp     DSP, parameters, patch-bank logic, MIDI handling
  PluginEditor.h/.cpp        UI (hardware-inspired layout)
  GrooveTapLookAndFeel.h     Custom image-based knob/button rendering
  FactoryPresets.h/.cpp      The 90 factory patches (see disclaimer above)
Resources/
  PanelBackground.png        Faceplate texture (AI-generated)
  Knob.png, Button.png       Knob/footswitch artwork (AI-generated)
CMakeLists.txt
```

## License

The original code in this repository (everything except the factory preset
*values* transcribed from Yamaha's manual, see disclaimer above) is
provided as-is for personal/educational use. No warranty of any kind.
