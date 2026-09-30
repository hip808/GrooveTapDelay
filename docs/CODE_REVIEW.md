# Code review and 1.1 changes

## Implemented

- Replaced the generic editor with MOKU AI branding, teal textured panels, cream legends, rotary knobs, amber preset display and four footswitch-style controls. Resizes proportionally from 1000×667 to 1800×1200.
- Added eight independently host-automatable band ON/OFF buttons. Band selection remains separate. Existing parameter IDs and plugin manufacturer/product codes remain unchanged.
- Added an Assign knob with a target menu for the selected band. This is a UI alias for an existing parameter, not a second automation parameter. Its assignment is editor-local and resets on reopening.
- Added Save-to-user-slot from factory patches, safe asynchronous menu callbacks, tap-gesture notification, clock-mode tap disabling, and a last-tap BPM readout. A tap interval is capped at the existing 696 ms per-band limit.
- Fixed numeric text entry for 0–10 controls, percent TAP, L/R pan and cutoff frequencies. Typed display values now convert back into parameter units.
- Fixed mono-to-stereo processing: the right channel reads the mono input rather than the output-only buffer.
- Fixed zero/sub-sample delay buffer reads with a one-sample causal minimum. Zero does not mean instantaneous wet pass-through.
- Fixed disabled bands freezing their buffers: they now advance with zero input and decay silently.
- Fixed an all-disabled series chain leaking dry audio through the wet path.
- Made OFF filters bypass completely, clamped active cutoff calculations below Nyquist, and preallocated/reused filter coefficients instead of allocating each audio block.
- Cached per-band rate, depth, time and pan calculations per block to remove repeated powers/square roots and parameter reads from the sample loop.
- Added processor reset handling for delay/filter/LFO state and a parameter-dependent tail estimate instead of a fixed six-second tail.
- Fixed unsaved renamed-user-patch names disappearing on serialization, stale bank slots surviving a new restore, out-of-range restored patch indices, and factory-slot mutation through restored bank entries.
- Serialized patch-bank access with a lock (outside processBlock); the current-patch index is atomic.
- Added a configurable JUCE_PATH, opt-in installation, regression-test target and build/test instructions.

## Compatibility and audible changes

The CMake target, manufacturer code Alnp, plugin code Gtd1, parameter IDs/ranges, PARAMS tree and GrooveTapState XML root are retained. The display/product name is now MOKU AI UD Stomp. Existing sessions are intended to restore, but DAW-specific rescan/session migration has not been tested. Treat this as an update to GrooveTap, not a separately identified plugin to install alongside it.

The bug fixes deliberately change OFF-filter feedback, zero-delay behavior, re-enabled bands and all-disabled series output. Existing routing, TAP semantics, approximate LFO curves and filter topology otherwise remain. The hardware-fidelity assessment documents mismatches; this release does not silently substitute an uncalibrated hardware model.

## Validation

Automated regression coverage includes parameter text conversion; mono bypass with a deliberately poisoned output channel; zero-delay impulse timing; unfiltered repeat amplitude; all-disabled series silence; muted-band decay; reset; long-feedback tail reporting; finite output at 22.05/44.1/48/96/192 kHz; bank rename/restore; all 90 factory entries in short impulse smoke checks; and the editor's independent band attachment. UI images are snapshots rendered by the actual JUCE editor, not generated concept art.

## Remaining improvements

- Parameter changes, routing changes and band/bypass switches are not smoothed; add ramps/crossfades with discontinuity tests.
- Delay modulation still uses linear interpolation. Compare alternatives only against measured hardware spectra/phase.
- Long host-clock divisions clamp at 696 ms; the displayed division may not be achievable at a given tempo. A grouped-delay implementation should expose a clear range/limit indication.
- Preset loads notify many parameters sequentially, so an audio block can see a transitional combination during a preset change. Consider a coherent DSP snapshot/crossfade.
- The dry-path stereo balance convention and cascaded series gain need hardware calibration.
- No full third-party VST3 validator, DAW listening session, or physical UD-Stomp A/B has been run.

See HARDWARE_FIDELITY.md for the sourced signal-path assessment and measurement plan.
