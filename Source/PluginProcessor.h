#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "FactoryPresets.h"

// Patch bank layout: 0..89 = read-only Factory (from the real manual data),
// 90..179 = User bank (starts as an editable copy of the matching Factory
// patch, exactly like the hardware's 90+90 split), 180..998 = extra free
// User slots beyond what the original hardware offered.
static constexpr int kNumFactorySlots = kNumFactoryPresets; // 90
static constexpr int kNumUserSlots = kNumFactoryPresets;    // 90
static constexpr int kTotalPatches = 999;

static constexpr int kNumBands = 8;
static constexpr float kMaxBandTimeMs = 696.0f;

enum class LfoWave
{
    Sine = 0, Triangle, SawUp, SawDown,
    SineTriangle, SineSawUp, SineSawDown,
    Count
};

// One independent delay line: its own time, feedback, loop filter, pan,
// level and its own LFO for chorus/flange/pitch-mod style modulation.
struct DelayBand
{
    std::vector<float> bufL, bufR;
    int bufLen = 1;
    int writePos = 0;

    double lfoPhase = 0.0;

    juce::dsp::IIR::Filter<float> loopLowCutL, loopLowCutR;   // high-pass (cuts lows)
    juce::dsp::IIR::Filter<float> loopHighCutL, loopHighCutR; // low-pass  (cuts highs)

    // Smoothed per-sample gains, so Feedback/Level/Pan knob moves and host
    // automation ramp instead of jumping (jumping is what causes clicks).
    juce::SmoothedValue<float> smFeedback, smLevel, smPanL, smPanR;
    bool smoothingInitialized = false;

    void prepare (double sampleRate)
    {
        bufLen = (int) std::ceil ((kMaxBandTimeMs / 1000.0 + 0.03) * sampleRate) + 8;
        bufL.assign ((size_t) bufLen, 0.0f);
        bufR.assign ((size_t) bufLen, 0.0f);
        writePos = 0;
        lfoPhase = 0.0;

        juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
        loopLowCutL.prepare (spec);
        loopLowCutR.prepare (spec);
        loopHighCutL.prepare (spec);
        loopHighCutR.prepare (spec);

        constexpr double smoothSeconds = 0.02;
        smFeedback.reset (sampleRate, smoothSeconds);
        smLevel.reset (sampleRate, smoothSeconds);
        smPanL.reset (sampleRate, smoothSeconds);
        smPanR.reset (sampleRate, smoothSeconds);
        smoothingInitialized = false; // next processBlock snaps to the current value instead of ramping from 0
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        writePos = 0;
        loopLowCutL.reset();
        loopLowCutR.reset();
        loopHighCutL.reset();
        loopHighCutR.reset();
    }
};

class GrooveTapDelayAudioProcessor : public juce::AudioProcessor
{
public:
    GrooveTapDelayAudioProcessor();
    ~GrooveTapDelayAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MOKU AI 8Tap"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    // Default MIDI note numbers mirroring the hardware's 4 footswitches:
    // Select Band (cycle), Band On/Off (toggle the selected band), Tap
    // Tempo, Bypass. Fixed for now - no MIDI-learn UI yet.
    // Deliberately kept high (above nearly all real musical content,
    // including the near-universal "kick drum = note 36/C1" convention)
    // to avoid a normal MIDI part on this track silently triggering these.
    static constexpr int kMidiNoteSelectBand = 116;
    static constexpr int kMidiNoteToggleOnOff = 117;
    static constexpr int kMidiNoteTapTempo = 118;
    static constexpr int kMidiNoteBypass = 119;

    int getMidiSelectedBand() const { return midiSelectedBand.load(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::String bandParamId (int bandIndex, const char* suffix)
    {
        return "band" + juce::String (bandIndex + 1) + "_" + suffix;
    }

    static const juce::StringArray waveChoices;
    static const juce::StringArray topologyChoices;
    static const juce::StringArray clockChoices;

    // --- Patch / preset bank ---
    int getNumPatches() const { return kTotalPatches; }
    int getCurrentPatchIndex() const { return currentPatchIndex; }
    juce::String getPatchName (int index) const;
    bool isPatchEditable (int index) const { return index >= kNumFactorySlots; }
    void loadPatch (int index);
    void savePatch (int index, const juce::String& newName);
    void renamePatch (int index, const juce::String& newName);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;

    std::array<DelayBand, kNumBands> bands;

    struct BandParamPtrs
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* timeMs = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* lfoSpeed = nullptr;
        std::atomic<float>* lfoDepth = nullptr;
        std::atomic<float>* lfoWave = nullptr;
        std::atomic<float>* clockSync = nullptr;
        std::atomic<float>* syncPercent = nullptr;
    };
    std::array<BandParamPtrs, kNumBands> bandParams;

    std::atomic<float>* topologyParam = nullptr;
    std::atomic<float>* effectLevelParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* directLevelParam = nullptr;
    std::atomic<float>* directPanParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    float computeLfoValue (double phase, int waveIndex) const;

    void applyFactoryPreset (const FactoryPreset& preset);
    void setRawParam (const juce::String& id, float rawValue);
    juce::String getDefaultPatchName (int index) const;

    juce::StringArray patchNames;
    juce::StringArray patchXmlStore; // per-slot saved ValueTree XML; empty = not yet saved
    int currentPatchIndex = 0;
    juce::String defaultStateXml;

    void handleMidiMessage (const juce::MidiMessage& m, int samplePositionInBlock);
    std::atomic<int> midiSelectedBand { 0 };
    juce::int64 totalSamplesProcessed = 0;
    juce::int64 lastTapSampleTime = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveTapDelayAudioProcessor)
};
