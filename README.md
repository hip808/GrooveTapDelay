# MOKU AI UD Stomp

An eight-band modulation/delay VST3 for macOS, built with JUCE. Formerly GrooveTap Delay.

The new editor follows the approved teal-metal stompbox design: separate band selection and ON/OFF rows, a preset display, an assignable control, two rows of parameter knobs, and four footswitch-style controls. The artwork is drawn at runtime and scales with the window.

This is an independent approximation, not an exact Yamaha hardware emulation. Read the [hardware-fidelity assessment](docs/HARDWARE_FIDELITY.md) for confirmed components, current DSP differences and the measurement plan. The [code review](docs/CODE_REVIEW.md) records fixes and remaining work.

## Controls

- Select 1–8 to edit a band; use the second ON/OFF row to enable any band without changing selection. Host automation and saved states use the existing band enable parameters.
- ASSIGN mirrors the selected band's Time, Feedback, Depth, Speed, Level, TAP or Pan. Choose its target from the menu beneath it; the target is editor-local.
- ROUTING chooses all-parallel or all-series. CLOCK selects host-tempo divisions for the current band; it is not hardware modulation sync. WAVE selects the existing LFO shapes.
- Footswitches: select next band, toggle selected band, tap its delay time, and global bypass. Tap is disabled when CLOCK controls time. The BPM display reports the last accepted tap interval (clamped to 696 ms).
- Double-click a value to type it. Use display units, e.g. 3.5, L5, 50%, or 1000 Hz. Double-click the patch number to jump to a slot.
- SAVE on a factory patch opens a user-slot destination menu. SAVE on a user patch stores in that slot. User names can be edited. User slots live in the DAW session state, not in a shared disk library.

## Engine and limitations

Eight independent stereo delay buffers, time up to 696 ms per band, feedback up to 0.95, loop filters, pan/level and per-band modulation. TAP scales the read delay; routing and grouping are simplified. Eight unmodulated 696 ms stages total 5.568 seconds, not the original hardware's grouped-memory implementation.

The 999-slot bank contains 90 factory entries, 90 editable initial copies, and 819 extra user slots. Factory values do not fully reproduce Yamaha patches because connection/group/sync/polarity metadata is absent. No MIDI input, loop sampling, fixed-44.1-kHz converter model or measured analog-stage model is implemented.

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

## Build and test

Requires CMake 3.22+, an installed macOS SDK/compiler, and a JUCE checkout (tested with JUCE 9.0.2). The default JUCE path is ~/dev/JUCE; override it with JUCE_PATH.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=/path/to/JUCE -DBUILD_TESTING=ON
cmake --build build -j 6
ctest --test-dir build --output-on-failure
```

Output: `build/GrooveTapDelay_artefacts/Release/VST3/MOKU AI UD Stomp.vst3`.
For a universal Mac build add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` at configure time.
Installation is opt-in: configure with `-DMOKU_COPY_PLUGIN=ON`, or copy the built bundle into your user VST3 folder and rescan the DAW.

The manufacturer/plugin codes are unchanged to retain the plugin's identity. Back up your existing bundle before installing this update; do not intentionally install both builds with the same identity. Parameter IDs and legacy state roots are retained, but host-specific session migration still needs a DAW check.

Generate an actual-editor snapshot with `build/MokuTests /absolute/path/preview.png`.

## Project layout

- `Source/PluginProcessor.*`: DSP, parameters and bank persistence
- `Source/PluginEditor.*`, `Source/StompLookAndFeel.h`: editor and procedural artwork
- `Source/FactoryPresets.*`: bundled preset table
- `Tests/RegressionTests.cpp`: audio/state/UI regression checks
- `docs/`: review findings and hardware research

## License

The original code (excluding factory preset values described above) is provided as-is for personal/educational use, without warranty.
