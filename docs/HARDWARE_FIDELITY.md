# UD-Stomp fidelity assessment

## Conclusion

MOKU AI UD Stomp 1.1 is an independently implemented delay with a hardware-inspired interface. It is **not yet an exact sonic emulation**. Matching the original requires correcting algorithmic differences and measuring a physical unit; lowering the bit depth alone cannot establish a match. No invented converter coloration, noise, or saturation has been added to this release. No physical reference unit is available for this project at present; exact-match claims must remain unverified until suitable captures can be obtained.

## Confirmed hardware

Yamaha specifies 44.1 kHz processing, a “20 bit + 3 bit Floating” A/D system and 20-bit D/A. The input is mono; the outputs are stereo. Bands support connection/grouping and modulation synchronization. The filters act on the first echo as well as subsequent repeats. TAP controls the output position within the delay loop. See the [Yamaha owner’s manual, pp. 8, 14–17, 29](https://usa.yamaha.com/files/download/other_assets/6/313746/udstomp_en.pdf).

The [Yamaha service manual, pp. 14–17 (archived transcript)](https://manualzilla.com/doc/7442686/yamaha-ud-stomp-service-manual) identifies the **AK4520A-VF-E2 ADC/DAC (IC104)**, **YSS910-S DSP6 (IC8)** and **NJM072M op-amp (IC101)**. Component identities are established; the complete analog transfer function and Yamaha’s floating-input implementation have not been verified from a legible schematic. Do not interpret “20 + 3” as a conventional 23-bit linear converter.

The [AKM AK4520A datasheet, pp. 5–6](https://www.alldatasheet.jp/html-pdf/54946/AKM/AK4520A/250/5/AK4520A.html) specifies converter performance under stated test conditions, rather than the complete pedal’s performance. Its [filter table](https://www.alldatasheet.jp/html-pdf/54946/AKM/AK4520A/301/6/AK4520A.html) documents ADC and DAC filtering, including group delays of 29.3 and 14.7 samples. Their sum is approximately 1 ms at 44.1 kHz; that is not a measurement of total pedal latency.

## Differences found in this code

| Area | Current implementation | Consequence / next work |
|---|---|---|
| Routing | A global parallel/series switch; factory loading forces parallel | Rebuild per-band connections and groups before expecting routing-dependent presets to match. |
| Factory data | Struct lacks connection, grouping, polarity and modulation-sync fields; delay is clamped to 696 ms | Re-extract and validate missing columns against the official patch list. Long grouped patches cannot currently be faithfully represented. |
| TAP | Multiplies the single read delay used for both audible output and feedback | Separate output-tap position from loop length, verify with impulse captures. Preserve old sessions through versioned DSP behavior. |
| Filters | Existing approximate second-order filters are applied in the feedback path only | Move/duplicate filtering as required to match first-echo behavior, then measure cutoff curves, order and phase. |
| LFO | Inferred 0.02–8 Hz rate and squared depth up to 15 ms; mixed waves are averaged | Measure rate, excursion, phase relationships, waveform shape, and dual-wave construction. |
| Clock | Host BPM subdivisions | This is a DAW feature, not Yamaha inter-band modulation synchronization. The UI says CLOCK accordingly. |
| Stereo/pan | Independent stereo delay buffers, constant-power balance gains | Establish a mono-in hardware mode and measure pan/level laws. Existing stereo support should remain a separate mode. |
| Feedback | Linear gain capped at 0.95 | Measure the control taper; this implementation does not provide the hardware’s maximum-feedback behavior. |
| Converters | Host-rate floating-point audio; no converter/analog model | Add a measured 44.1 kHz reference path with quality resampling and latency reporting after algorithm calibration. |
| Switching | Immediate enable/bypass and unsmoothed parameter changes | Add tested crossfades/smoothing; distinguish band mute, input mute with tail spillover, and bypass. |

The [official Yamaha patch list](https://usa.yamaha.com/files/download/other_assets/7/313747/udstomp_en2.pdf) includes columns for CONNECT, PHASE, GROUP and SYNC that the present preset struct omits. Merely retaining familiar patch names therefore does not reproduce those patches.

## Recommended measurement sequence

These are engineering proposals, not claims about measured hardware behavior.

1. **Establish a calibrated reference.** Use a real UD-Stomp, a reamp/level-matching path, and a known interface. Record mono excitation with both analog outputs and, if available, S/PDIF at 44.1 kHz. Record the input-level switch, patch, all controls and signal levels. First measure the recording chain itself.
2. **Identify the linear engine.** Capture impulses for isolated bands with modulation and feedback disabled. Sweep time/TAP, pan and level. Then add feedback and filters one at a time. Recover timing, frequency response, decay and channel gains.
3. **Identify modulation.** Use sustained tones and chirps over several LFO periods. Fit speed/depth curves, phase relationships and interpolation response; do not infer them from knob numbers alone.
4. **Validate grouped/routed patches.** Reconstruct the graph and preset metadata, then compare impulse locations and stereo envelopes. Use both simple diagnostic patches and representative factory presets.
5. **Measure the hardware contribution.** Compare level-aligned digital and analog captures, accounting for the recording interface. Use level sweeps, low-level tones and silence to estimate bandwidth, distortion, clipping and noise. The digital output still includes the input A/D path, so it does not isolate the ADC by itself.
6. **Implement and validate.** Keep algorithm, ADC/input and DAC/output models separate. Use fixed-44.1 kHz processing with proper sample-rate conversion where measurements justify it. Do not repeatedly apply an ADC/DAC model inside the feedback loop. Validate held-out recordings with timing, spectral and envelope errors plus level-matched listening; free-running modulation may prevent sample-perfect nulling.

Priority: routing/TAP/filters and modulation first, then measured converter and analog behavior. This prioritization is an inference from the identified code differences, not an assertion that the converter is inaudible.
