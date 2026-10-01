#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray GrooveTapDelayAudioProcessor::waveChoices {
    "Sine", "Triangle", "Saw Up", "Saw Down",
    "Sine + Triangle", "Sine + Saw Up", "Sine + Saw Down"
};

const juce::StringArray GrooveTapDelayAudioProcessor::topologyChoices { "Parallel", "Series" };

// Index 0 ("Off") means the band ignores host tempo and just uses its Time
// knob (ms) directly. Indices 1+ derive the delay time from the host's
// MIDI/transport clock (beats-per-minute) at that note division.
const juce::StringArray GrooveTapDelayAudioProcessor::clockChoices {
    "Off", "1/32", "1/16", "1/16 Dotted", "1/8", "1/8 Dotted",
    "1/4", "1/4 Dotted", "1/2", "1 Bar", "2 Bar"
};
static const double clockBeatLengths[] = { 0.0, 0.125, 0.25, 0.375, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0, 8.0 };

static float sineWave (double p)     { return (float) std::sin (p * juce::MathConstants<double>::twoPi); }
static float triangleWave (double p) { return (float) (4.0 * std::abs (p - std::floor (p + 0.75) + 0.25) - 1.0); }
static float sawUpWave (double p)    { return (float) (2.0 * (p - std::floor (p + 0.5))); }
static float sawDownWave (double p)  { return -sawUpWave (p); }

float GrooveTapDelayAudioProcessor::computeLfoValue (double phase, int waveIndex) const
{
    switch ((LfoWave) waveIndex)
    {
        case LfoWave::Sine:         return sineWave (phase);
        case LfoWave::Triangle:     return triangleWave (phase);
        case LfoWave::SawUp:        return sawUpWave (phase);
        case LfoWave::SawDown:      return sawDownWave (phase);
        case LfoWave::SineTriangle: return 0.5f * (sineWave (phase) + triangleWave (phase));
        case LfoWave::SineSawUp:    return 0.5f * (sineWave (phase) + sawUpWave (phase));
        case LfoWave::SineSawDown:  return 0.5f * (sineWave (phase) + sawDownWave (phase));
        default: return 0.0f;
    }
}

GrooveTapDelayAudioProcessor::GrooveTapDelayAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& bp = bandParams[(size_t) b];
        bp.enabled  = apvts.getRawParameterValue (bandParamId (b, "enabled"));
        bp.timeMs   = apvts.getRawParameterValue (bandParamId (b, "time"));
        bp.feedback = apvts.getRawParameterValue (bandParamId (b, "feedback"));
        bp.lowCut   = apvts.getRawParameterValue (bandParamId (b, "lowcut"));
        bp.highCut  = apvts.getRawParameterValue (bandParamId (b, "highcut"));
        bp.pan      = apvts.getRawParameterValue (bandParamId (b, "pan"));
        bp.level    = apvts.getRawParameterValue (bandParamId (b, "level"));
        bp.lfoSpeed = apvts.getRawParameterValue (bandParamId (b, "lfospeed"));
        bp.lfoDepth = apvts.getRawParameterValue (bandParamId (b, "lfodepth"));
        bp.lfoWave  = apvts.getRawParameterValue (bandParamId (b, "lfowave"));
        bp.clockSync   = apvts.getRawParameterValue (bandParamId (b, "clock"));
        bp.syncPercent = apvts.getRawParameterValue (bandParamId (b, "syncpct"));
    }

    topologyParam    = apvts.getRawParameterValue ("topology");
    effectLevelParam = apvts.getRawParameterValue ("effectLevel");
    outputParam      = apvts.getRawParameterValue ("output");
    directLevelParam = apvts.getRawParameterValue ("directLevel");
    directPanParam   = apvts.getRawParameterValue ("directPan");
    bypassParam      = apvts.getRawParameterValue ("bypass");

    // Capture the just-constructed default parameter state once, so any
    // never-saved extra patch slot (180+) has a sane "Init" state to load.
    defaultStateXml = apvts.copyState().createXml()->toString();

    patchNames.ensureStorageAllocated (kTotalPatches);
    patchXmlStore.ensureStorageAllocated (kTotalPatches);
    for (int i = 0; i < kTotalPatches; ++i)
    {
        patchNames.add (getDefaultPatchName (i));
        patchXmlStore.add ({});
    }

    // Fresh instances start sounding like Factory patch 001, matching what
    // the patch display shows. A restored session's setStateInformation()
    // (called by the host right after construction) overrides this.
    loadPatch (0);
}

GrooveTapDelayAudioProcessor::~GrooveTapDelayAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout GrooveTapDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct10 = [] (float v, int) { return juce::String (v * 10.0f, 1); };
    auto msText = [] (float v, int) { return juce::String ((int) std::round (v)) + " ms"; };
    auto hzOffText0to1LowCut = [] (float v, int)
    {
        if (v < 0.02f) return juce::String ("OFF");
        float hz = juce::jmap (v, 0.0f, 1.0f, 20.0f, 1000.0f);
        return juce::String ((int) hz) + " Hz";
    };
    auto hzOffText0to1HighCut = [] (float v, int)
    {
        if (v < 0.02f) return juce::String ("OFF");
        float hz = juce::jmap (v, 0.0f, 1.0f, 20000.0f, 1000.0f);
        return juce::String ((int) hz) + " Hz";
    };
    auto panText = [] (float v, int)
    {
        if (std::abs (v) < 0.02f) return juce::String ("C");
        return v < 0 ? ("L" + juce::String (juce::roundToInt (-v * 10))) : ("R" + juce::String (juce::roundToInt (v * 10)));
    };

    for (int b = 0; b < kNumBands; ++b)
    {
        bool defaultOn = (b == 0);
        float defaultTime = juce::jmin (kMaxBandTimeMs, 120.0f + (float) b * 70.0f);

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            bandParamId (b, "enabled"), "Band " + juce::String (b + 1) + " On", defaultOn));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "time"), "Band " + juce::String (b + 1) + " Time",
            juce::NormalisableRange<float> (0.0f, kMaxBandTimeMs), defaultTime,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "feedback"), "Band " + juce::String (b + 1) + " Feedback",
            juce::NormalisableRange<float> (0.0f, 0.95f), 0.30f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "lowcut"), "Band " + juce::String (b + 1) + " Low Cut",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (hzOffText0to1LowCut)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "highcut"), "Band " + juce::String (b + 1) + " High Cut",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (hzOffText0to1HighCut)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "pan"), "Band " + juce::String (b + 1) + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f), (b % 2 == 0) ? -0.4f : 0.4f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (panText)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "level"), "Band " + juce::String (b + 1) + " Level",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "lfospeed"), "Band " + juce::String (b + 1) + " LFO Speed",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "lfodepth"), "Band " + juce::String (b + 1) + " LFO Depth",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10)));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            bandParamId (b, "lfowave"), "Band " + juce::String (b + 1) + " Wave", waveChoices, 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            bandParamId (b, "clock"), "Band " + juce::String (b + 1) + " Clock", clockChoices, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "syncpct"), "Band " + juce::String (b + 1) + " Sync %",
            juce::NormalisableRange<float> (0.05f, 2.0f), 1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [](float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
    }

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("topology", "Topology", topologyChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "effectLevel", "Effect Level", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "output", "Output", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "directLevel", "Direct Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "directPan", "Direct Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (panText)));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

void GrooveTapDelayAudioProcessor::prepareToPlay (double sr, int)
{
    currentSampleRate = sr;
    for (auto& band : bands)
        band.prepare (sr);
}

void GrooveTapDelayAudioProcessor::releaseResources() {}

void GrooveTapDelayAudioProcessor::reset()
{
    // Hosts call this on transport stop/relocate/loop - clear delay buffers
    // and filter state so old audio can't bleed across the discontinuity.
    for (auto& band : bands)
        band.reset();
}

bool GrooveTapDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto mono = juce::AudioChannelSet::mono();
    auto stereo = juce::AudioChannelSet::stereo();
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    return (in == stereo || in == mono) && out == stereo;
}

static inline float readInterpolated (const std::vector<float>& buf, int bufLen, float readPosF)
{
    // A single fmod-based wrap instead of a while-loop: bounded cost
    // regardless of how far negative readPosF is (defence in depth - it
    // shouldn't ever be more than one bufLen negative in practice).
    float wrapped = std::fmod (readPosF, (float) bufLen);
    if (wrapped < 0.0f) wrapped += (float) bufLen;
    int i0 = (int) wrapped;
    int i1 = (i0 + 1) % bufLen;
    float frac = wrapped - (float) i0;
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void GrooveTapDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto metadata : midiMessages)
        handleMidiMessage (metadata.getMessage(), metadata.samplePosition);

    bool isBypassed = bypassParam->load() > 0.5f;
    bool isSeries = (int) std::round (topologyParam->load()) == 1;

    double hostBpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm = *bpm;
    hostBpm = juce::jmax (hostBpm, 20.0);
    float outGain = juce::Decibels::decibelsToGain (outputParam->load());
    float directLevel = directLevelParam->load();
    float directPan = directPanParam->load();
    float directPanL = std::sqrt (0.5f * (1.0f - directPan));
    float directPanR = std::sqrt (0.5f * (1.0f + directPan));

    auto* inL = buffer.getReadPointer (0);
    auto* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0);
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    int numSamples = buffer.getNumSamples();

    // Cache per-band filter coefficients once per block (parameters rarely change per-sample).
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& bp = bandParams[(size_t) b];
        auto& band = bands[(size_t) b];

        float lowCutNorm = bp.lowCut->load();
        float highCutNorm = bp.highCut->load();

        float lowCutHz = lowCutNorm < 0.02f ? 20.0f : juce::jmap (lowCutNorm, 0.0f, 1.0f, 20.0f, 1000.0f);
        float highCutHz = highCutNorm < 0.02f ? 20000.0f : juce::jmap (highCutNorm, 0.0f, 1.0f, 20000.0f, 1000.0f);

        // Push this block's Feedback/Level/Pan targets into the smoothers
        // (snap on the very first block so playback doesn't ramp up from 0).
        float targetFeedback = bp.feedback->load();
        float targetLevel = bp.level->load();
        float targetPan = bp.pan->load();
        float targetPanL = std::sqrt (0.5f * (1.0f - targetPan));
        float targetPanR = std::sqrt (0.5f * (1.0f + targetPan));
        if (! band.smoothingInitialized)
        {
            band.smFeedback.setCurrentAndTargetValue (targetFeedback);
            band.smLevel.setCurrentAndTargetValue (targetLevel);
            band.smPanL.setCurrentAndTargetValue (targetPanL);
            band.smPanR.setCurrentAndTargetValue (targetPanR);
            band.smoothingInitialized = true;
        }
        else
        {
            band.smFeedback.setTargetValue (targetFeedback);
            band.smLevel.setTargetValue (targetLevel);
            band.smPanL.setTargetValue (targetPanL);
            band.smPanR.setTargetValue (targetPanR);
        }

        auto lc = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, lowCutHz);
        auto hc = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, highCutHz);

        band.loopLowCutL.coefficients = lc;
        band.loopLowCutR.coefficients = lc;
        band.loopHighCutL.coefficients = hc;
        band.loopHighCutR.coefficients = hc;
    }

    constexpr float kMaxModMs = 15.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        float dryL = inL[n];
        float dryR = inR[n];

        float stageL = dryL;
        float stageR = dryR;

        float wetSumL = 0.0f;
        float wetSumR = 0.0f;

        for (int b = 0; b < kNumBands; ++b)
        {
            auto& bp = bandParams[(size_t) b];
            auto& band = bands[(size_t) b];

            bool enabled = bp.enabled->load() > 0.5f;

            float bandInL = isSeries ? stageL : dryL;
            float bandInR = isSeries ? stageR : dryR;

            if (! enabled)
            {
                if (isSeries) { /* pass through unchanged */ }
                continue;
            }

            // Advance this band's own LFO. Both Speed and Depth are 0-10
            // knobs on the hardware; a musically-sane rate/depth range spans
            // more than one octave, so - like almost every analog modulation
            // rate/depth control - the knob position is curved (exponential
            // for rate, power-law for depth) rather than mapped linearly.
            // A raw value of "3.5" should feel like a gentle chorus, not
            // a third of the way to a fast vibrato.
            float speedNorm = bp.lfoSpeed->load();
            float speedHz = 0.02f * std::pow (400.0f, speedNorm); // 0.02 Hz .. 8 Hz, exponential
            band.lfoPhase += speedHz / currentSampleRate;
            if (band.lfoPhase >= 1.0) band.lfoPhase -= 1.0;

            int waveIdx = juce::jlimit (0, (int) LfoWave::Count - 1, (int) std::round (bp.lfoWave->load()));
            float lfoVal = computeLfoValue (band.lfoPhase, waveIdx);

            float depthNorm = bp.lfoDepth->load();
            float depthCurved = depthNorm * depthNorm; // power-law taper: gentle at low settings
            float modMs = lfoVal * depthCurved * kMaxModMs;

            int clockIdx = juce::jlimit (0, clockChoices.size() - 1, (int) std::round (bp.clockSync->load()));
            float syncPct = bp.syncPercent->load();
            float fullTimeMs;
            if (clockIdx == 0)
                fullTimeMs = bp.timeMs->load();
            else
                fullTimeMs = (float) ((60000.0 / hostBpm) * clockBeatLengths[clockIdx]);
            fullTimeMs = juce::jlimit (0.0f, kMaxBandTimeMs, fullTimeMs);

            // TAP (the hardware's real per-band control): "the timing at
            // which the delayed sound is taken from the delay loop,
            // expressed as a percentage of the delay time setting." The
            // feedback loop itself always recirculates at the full
            // fullTimeMs length; TAP only moves where the audible output
            // is read from within that loop - it does not shorten the loop.
            float tapTimeMs = juce::jlimit (0.0f, kMaxBandTimeMs, fullTimeMs * syncPct);
            float tapTimeModMs = juce::jlimit (0.0f, kMaxBandTimeMs + kMaxModMs, tapTimeMs + modMs);

            float delaySamplesFull = (float) (fullTimeMs / 1000.0 * currentSampleRate);
            float delaySamplesTap = (float) (tapTimeModMs / 1000.0 * currentSampleRate);

            float readFullL = readInterpolated (band.bufL, band.bufLen, (float) band.writePos - delaySamplesFull);
            float readFullR = readInterpolated (band.bufR, band.bufLen, (float) band.writePos - delaySamplesFull);
            float readTapL = readInterpolated (band.bufL, band.bufLen, (float) band.writePos - delaySamplesTap);
            float readTapR = readInterpolated (band.bufR, band.bufLen, (float) band.writePos - delaySamplesTap);

            float feedback = band.smFeedback.getNextValue();

            // The loop filter sits in the write path, so every sample is
            // shaped as it enters the loop - including the very first pass
            // (the manual: "the cutoff is also applied to the first delay"),
            // with cumulative darkening on each subsequent repeat.
            float combinedL = bandInL + readFullL * feedback;
            float combinedR = bandInR + readFullR * feedback;
            float filteredL = band.loopHighCutL.processSample (band.loopLowCutL.processSample (combinedL));
            float filteredR = band.loopHighCutR.processSample (band.loopLowCutR.processSample (combinedR));

            band.bufL[(size_t) band.writePos] = filteredL;
            band.bufR[(size_t) band.writePos] = filteredR;
            band.writePos = (band.writePos + 1) % band.bufLen;

            float level = band.smLevel.getNextValue();
            float panL = band.smPanL.getNextValue();
            float panR = band.smPanR.getNextValue();

            float bandOutL = readTapL * level * panL;
            float bandOutR = readTapR * level * panR;

            if (isSeries)
            {
                stageL = bandOutL;
                stageR = bandOutR;
            }
            else
            {
                wetSumL += bandOutL;
                wetSumR += bandOutR;
            }
        }

        float effectLevel = effectLevelParam->load();
        float wetL = (isSeries ? stageL : wetSumL) * effectLevel;
        float wetR = (isSeries ? stageR : wetSumR) * effectLevel;

        float finalL = (dryL * directPanL * directLevel + wetL) * outGain;
        float finalR = (dryR * directPanR * directLevel + wetR) * outGain;

        outL[n] = isBypassed ? dryL : finalL;
        outR[n] = isBypassed ? dryR : finalR;
    }

    totalSamplesProcessed += numSamples;
}

void GrooveTapDelayAudioProcessor::handleMidiMessage (const juce::MidiMessage& m, int samplePositionInBlock)
{
    if (! m.isNoteOn()) return;

    int note = m.getNoteNumber();

    if (note == kMidiNoteSelectBand)
    {
        midiSelectedBand.store ((midiSelectedBand.load() + 1) % kNumBands);
    }
    else if (note == kMidiNoteToggleOnOff)
    {
        int b = midiSelectedBand.load();
        auto id = bandParamId (b, "enabled");
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    }
    else if (note == kMidiNoteTapTempo)
    {
        // Sample-accurate interval measurement (avoids a wall-clock call on
        // the audio thread) - mirrors the UI's own double-tap tap-tempo.
        juce::int64 tapSampleTime = totalSamplesProcessed + samplePositionInBlock;
        if (lastTapSampleTime >= 0)
        {
            juce::int64 intervalSamples = tapSampleTime - lastTapSampleTime;
            double intervalMs = 1000.0 * (double) intervalSamples / currentSampleRate;
            if (intervalMs > 50.0 && intervalMs < 3000.0)
            {
                int b = midiSelectedBand.load();
                auto id = bandParamId (b, "time");
                if (auto* p = apvts.getParameter (id))
                {
                    float clamped = juce::jlimit (0.0f, kMaxBandTimeMs, (float) intervalMs);
                    p->setValueNotifyingHost (p->convertTo0to1 (clamped));
                }
            }
        }
        lastTapSampleTime = tapSampleTime;
    }
    else if (note == kMidiNoteBypass)
    {
        if (auto* p = apvts.getParameter ("bypass"))
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    }
}

juce::AudioProcessorEditor* GrooveTapDelayAudioProcessor::createEditor()
{
    return new GrooveTapDelayAudioProcessorEditor (*this);
}

void GrooveTapDelayAudioProcessor::setRawParam (const juce::String& id, float rawValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (rawValue));
}

void GrooveTapDelayAudioProcessor::applyFactoryPreset (const FactoryPreset& preset)
{
    // Reset everything this preset table doesn't cover to a sane baseline
    // first, so no state from a previously-loaded patch leaks through.
    setRawParam ("topology", 0.0f);
    setRawParam ("output", 0.0f);
    setRawParam ("bypass", 0.0f);
    setRawParam ("effectLevel", preset.effectLevel);
    setRawParam ("directLevel", preset.directLevel);
    setRawParam ("directPan", preset.directPan);

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& src = preset.bands[(size_t) b];
        setRawParam (bandParamId (b, "enabled"), src.enabled ? 1.0f : 0.0f);
        setRawParam (bandParamId (b, "time"), src.timeMs);

        // All of these are plain 0-10 knobs per the manual's own control
        // descriptions - convert by dividing by 10, same as every other
        // 0-10 knob (this used to be wrong for Speed, which was mistakenly
        // treated as a literal Hz value).
        setRawParam (bandParamId (b, "feedback"), juce::jlimit (0.0f, 0.95f, src.feedback10 / 10.0f));
        setRawParam (bandParamId (b, "lowcut"), juce::jlimit (0.0f, 1.0f, src.lowCut10 / 10.0f));
        setRawParam (bandParamId (b, "highcut"), juce::jlimit (0.0f, 1.0f, src.highCut10 / 10.0f));
        setRawParam (bandParamId (b, "pan"), src.pan);
        setRawParam (bandParamId (b, "level"), juce::jlimit (0.0f, 1.0f, src.level10 / 10.0f));
        setRawParam (bandParamId (b, "lfospeed"), juce::jlimit (0.0f, 1.0f, src.speed10 / 10.0f));
        setRawParam (bandParamId (b, "lfodepth"), juce::jlimit (0.0f, 1.0f, src.depth10 / 10.0f));
        setRawParam (bandParamId (b, "lfowave"), (float) src.waveIndex);

        setRawParam (bandParamId (b, "clock"), 0.0f); // Off - factory data predates our clock-sync addition

        // The manual's TAP knob is "percentage of the delay time setting"
        // (0-100%) - the real per-band control our Sync % knob was
        // repurposed from. Feed it through directly, widened/clamped to our
        // knob's actual range.
        setRawParam (bandParamId (b, "syncpct"), juce::jlimit (0.05f, 2.0f, src.tapPct / 100.0f));
    }
}

juce::String GrooveTapDelayAudioProcessor::getPatchName (int index) const
{
    if (index < 0 || index >= kTotalPatches) return {};
    return patchNames[index];
}

juce::String GrooveTapDelayAudioProcessor::getDefaultPatchName (int index) const
{
    if (index < kNumFactorySlots)
        return factoryPresets[(size_t) index].name;
    if (index < kNumFactorySlots + kNumUserSlots)
        return factoryPresets[(size_t) (index - kNumFactorySlots)].name;
    return "Init " + juce::String (index - (kNumFactorySlots + kNumUserSlots) + 1);
}

void GrooveTapDelayAudioProcessor::loadPatch (int index)
{
    index = juce::jlimit (0, kTotalPatches - 1, index);
    currentPatchIndex = index;

    if (index < kNumFactorySlots)
    {
        applyFactoryPreset (factoryPresets[(size_t) index]);
        return;
    }

    if (patchXmlStore[index].isNotEmpty())
    {
        if (auto xml = juce::XmlDocument::parse (patchXmlStore[index]))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        return;
    }

    if (index < kNumFactorySlots + kNumUserSlots)
    {
        // Never-saved User slot: starts out as a copy of the matching Factory patch.
        applyFactoryPreset (factoryPresets[(size_t) (index - kNumFactorySlots)]);
        return;
    }

    // Never-saved extra slot: load the plugin's original default ("Init") state.
    if (auto xml = juce::XmlDocument::parse (defaultStateXml))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void GrooveTapDelayAudioProcessor::savePatch (int index, const juce::String& newName)
{
    if (! isPatchEditable (index)) return; // Factory bank is read-only
    index = juce::jlimit (0, kTotalPatches - 1, index);
    patchXmlStore.set (index, apvts.copyState().createXml()->toString());
    patchNames.set (index, newName.isNotEmpty() ? newName : patchNames[index]);
}

void GrooveTapDelayAudioProcessor::renamePatch (int index, const juce::String& newName)
{
    if (! isPatchEditable (index) || newName.isEmpty()) return;
    patchNames.set (juce::jlimit (0, kTotalPatches - 1, index), newName);
}

void GrooveTapDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("GrooveTapState");
    root.setAttribute ("currentPatch", currentPatchIndex);

    if (auto paramsXml = apvts.copyState().createXml())
        root.addChildElement (paramsXml.release());

    auto* bank = new juce::XmlElement ("PatchBank");
    for (int i = kNumFactorySlots; i < kTotalPatches; ++i)
    {
        bool nameChanged = patchNames[i] != getDefaultPatchName (i);
        bool hasSavedState = patchXmlStore[i].isNotEmpty();
        if (! nameChanged && ! hasSavedState) continue;

        auto* slot = new juce::XmlElement ("Patch");
        slot->setAttribute ("index", i);
        slot->setAttribute ("name", patchNames[i]);
        if (hasSavedState)
            if (auto inner = juce::XmlDocument::parse (patchXmlStore[i]))
                slot->addChildElement (inner.release());
        bank->addChildElement (slot);
    }
    root.addChildElement (bank);

    copyXmlToBinary (root, destData);
}

void GrooveTapDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;

    if (xml->hasTagName ("GrooveTapState"))
    {
        currentPatchIndex = xml->getIntAttribute ("currentPatch", 0);

        if (auto* paramsXml = xml->getChildByName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*paramsXml));

        if (auto* bank = xml->getChildByName ("PatchBank"))
        {
            for (auto* slot : bank->getChildIterator())
            {
                int idx = slot->getIntAttribute ("index", -1);
                if (idx < 0 || idx >= kTotalPatches) continue;
                patchNames.set (idx, slot->getStringAttribute ("name"));
                if (auto* inner = slot->getFirstChildElement())
                    patchXmlStore.set (idx, inner->toString());
            }
        }
    }
    else if (xml->hasTagName (apvts.state.getType()))
    {
        // Backward-compat: older versions of this plugin saved just the bare
        // parameter-state XML with no patch bank.
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrooveTapDelayAudioProcessor();
}
