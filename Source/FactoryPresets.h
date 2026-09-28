#pragma once
#include <array>

static constexpr int kNumFactoryPresets = 90;

// Values here are the RAW knob units as printed in the manual's own patch
// list. Per the manual's control descriptions (SPEED/DEPTH/FEEDBACK/LEVEL/
// LOW-HIGH CUT are all plain 0-10 knobs; TAP is a 0-100% knob), every field
// here is 0..10 or 0..100 as noted, and gets converted into our normalised
// 0..1 parameter ranges when applied - see applyFactoryPreset().
struct FactoryPresetBand
{
    bool enabled;
    float timeMs;      // ms, used directly (matches our Time range)
    float feedback10;  // 0..10 knob units
    float lowCut10;    // 0..10 knob units (0 = off, 10 = 1kHz cutoff per manual)
    float highCut10;   // 0..10 knob units (0 = off, 10 = 1kHz cutoff per manual)
    float pan;         // already converted to -1..1
    float level10;     // 0..10 knob units
    float tapPct;      // 0..100, "percentage of the delay time" per manual
    float speed10;      // 0..10 knob units (NOT Hz, despite the name in earlier builds)
    float depth10;        // 0..10 knob units
    int waveIndex;        // 0 Sine, 1 Triangle, 2 SawUp, 3 SawDown
};

struct FactoryPreset
{
    const char* name;
    float effectLevel, directLevel, directPan;
    std::array<FactoryPresetBand, 8> bands;
};

extern const std::array<FactoryPreset, kNumFactoryPresets> factoryPresets;
