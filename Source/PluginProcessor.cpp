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
        case LfoWave::Count: return 0.0f;
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
        if (i < kNumFactorySlots)
            patchNames.add (factoryPresets[(size_t) i].name);
        else if (i < kNumFactorySlots + kNumUserSlots)
            patchNames.add (factoryPresets[(size_t) (i - kNumFactorySlots)].name);
        else
            patchNames.add ("Init " + juce::String (i - (kNumFactorySlots + kNumUserSlots) + 1));

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

    auto parsePan = [] (const juce::String& text)
    {
        auto value = text.trim().toUpperCase();
        if (value == "C") return 0.0f;
        if (value.startsWithChar ('L')) return -value.substring (1).getFloatValue() / 10.0f;
        if (value.startsWithChar ('R')) return value.substring (1).getFloatValue() / 10.0f;
        return value.getFloatValue() / 10.0f;
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
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 10.0f; })));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "lowcut"), "Band " + juce::String (b + 1) + " Low Cut",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (hzOffText0to1LowCut).withValueFromStringFunction ([] (const juce::String& text)
            { return text.trim().equalsIgnoreCase ("OFF") ? 0.0f : juce::jlimit (0.02f, 1.0f, (text.getFloatValue() - 20.0f) / 980.0f); })));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "highcut"), "Band " + juce::String (b + 1) + " High Cut",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (hzOffText0to1HighCut).withValueFromStringFunction ([] (const juce::String& text)
            { return text.trim().equalsIgnoreCase ("OFF") ? 0.0f : juce::jlimit (0.02f, 1.0f, (20000.0f - text.getFloatValue()) / 19000.0f); })));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "pan"), "Band " + juce::String (b + 1) + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f), (b % 2 == 0) ? -0.4f : 0.4f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (panText).withValueFromStringFunction (parsePan)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "level"), "Band " + juce::String (b + 1) + " Level",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 10.0f; })));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "lfospeed"), "Band " + juce::String (b + 1) + " LFO Speed",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 10.0f; })));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "lfodepth"), "Band " + juce::String (b + 1) + " LFO Depth",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 10.0f; })));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            bandParamId (b, "lfowave"), "Band " + juce::String (b + 1) + " Wave", waveChoices, 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            bandParamId (b, "clock"), "Band " + juce::String (b + 1) + " Clock", clockChoices, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            bandParamId (b, "syncpct"), "Band " + juce::String (b + 1) + " Sync %",
            juce::NormalisableRange<float> (0.05f, 2.0f), 1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [](float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 100.0f; })));
    }

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("topology", "Topology", topologyChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "effectLevel", "Effect Level", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 10.0f; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "output", "Output", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "directLevel", "Direct Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct10).withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue() / 10.0f; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "directPan", "Direct Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (panText).withValueFromStringFunction (parsePan)));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

void GrooveTapDelayAudioProcessor::prepareToPlay (double sr, int)
{
    currentSampleRate = sr;
    for (auto& band : bands)
        band.prepare (sr);
}

double GrooveTapDelayAudioProcessor::getTailLengthSeconds() const
{
    // Conservative -100 dB estimate. Clock-synced bands can reach the full delay
    // range; serial stages accumulate their individual decay times.
    double tail = 0;
    const bool series = topologyParam->load() > 0.5f;
    for (const auto& p : bandParams)
    {
        if (p.enabled->load() < 0.5f) continue;
        const auto delay = p.clockSync->load() > 0.5f ? kMaxBandTimeMs
            : juce::jlimit (0.0f, kMaxBandTimeMs, p.timeMs->load() * p.syncPercent->load());
        const auto feedback = (double) p.feedback->load();
        const auto repeats = feedback > 0.001 ? std::ceil (std::log (1.0e-5) / std::log (feedback)) : 0.0;
        const auto bandTail = ((double) delay + 15.0) * 0.001 * (1.0 + repeats);
        tail = series ? tail + bandTail : juce::jmax (tail, bandTail);
    }
    return tail;
}

void GrooveTapDelayAudioProcessor::releaseResources() {}

void GrooveTapDelayAudioProcessor::reset()
{
    for (auto& band : bands) band.reset();
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
    while (readPosF < 0.0f) readPosF += (float) bufLen;
    int i0 = (int) readPosF % bufLen;
    int i1 = (i0 + 1) % bufLen;
    float frac = readPosF - std::floor (readPosF);
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void GrooveTapDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    bool isBypassed = bypassParam->load() > 0.5f;
    bool isSeries = (int) std::round (topologyParam->load()) == 1;

    double hostBpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm = *bpm;
    hostBpm = std::isfinite (hostBpm) ? juce::jmax (hostBpm, 20.0) : 120.0;
    float outGain = juce::Decibels::decibelsToGain (outputParam->load());
    float directLevel = directLevelParam->load();
    float directPan = directPanParam->load();
    float directPanL = std::sqrt (0.5f * (1.0f - directPan));
    float directPanR = std::sqrt (0.5f * (1.0f + directPan));

    auto* inL = buffer.getReadPointer (0);
    auto* inR = getTotalNumInputChannels() > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0);
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

        // Fixed-size arrays and preallocated coefficient objects avoid heap work here.
        if (! juce::exactlyEqual (lowCutNorm, band.lastLowCut))
        {
            const auto lc = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (currentSampleRate, juce::jmin (lowCutHz, (float) currentSampleRate * 0.45f));
            *band.loopLowCutL.coefficients = lc;
            *band.loopLowCutR.coefficients = lc;
            if (lowCutNorm < 0.02f) { band.loopLowCutL.reset(); band.loopLowCutR.reset(); }
            band.lastLowCut = lowCutNorm;
        }
        if (! juce::exactlyEqual (highCutNorm, band.lastHighCut))
        {
            const auto hc = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (currentSampleRate, juce::jmin (highCutHz, (float) currentSampleRate * 0.45f));
            *band.loopHighCutL.coefficients = hc;
            *band.loopHighCutR.coefficients = hc;
            if (highCutNorm < 0.02f) { band.loopHighCutL.reset(); band.loopHighCutR.reset(); }
            band.lastHighCut = highCutNorm;
        }
    }

    constexpr float kMaxModMs = 15.0f;
    struct BlockBand
    {
        bool enabled;
        double phaseIncrement;
        int wave;
        float modulationMs, baseTimeMs, feedback, gainL, gainR;
    };
    std::array<BlockBand, kNumBands> settings;
    bool anyEnabled = false;
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& p = bandParams[(size_t) b];
        auto& s = settings[(size_t) b];
        s.enabled = p.enabled->load() > 0.5f;
        anyEnabled = anyEnabled || s.enabled;
        s.phaseIncrement = 0.02 * std::pow (400.0, (double) p.lfoSpeed->load()) / currentSampleRate;
        s.wave = juce::jlimit (0, (int) LfoWave::Count - 1, (int) std::round (p.lfoWave->load()));
        const auto depth = p.lfoDepth->load();
        s.modulationMs = depth * depth * kMaxModMs;
        const int clock = juce::jlimit (0, clockChoices.size() - 1, (int) std::round (p.clockSync->load()));
        const auto time = clock == 0 ? p.timeMs->load() : (float) (60000.0 / hostBpm * clockBeatLengths[clock]);
        s.baseTimeMs = juce::jlimit (0.0f, kMaxBandTimeMs, time * p.syncPercent->load());
        s.feedback = p.feedback->load();
        const auto pan = p.pan->load(), level = p.level->load();
        s.gainL = level * std::sqrt (0.5f * (1.0f - pan));
        s.gainR = level * std::sqrt (0.5f * (1.0f + pan));
    }
    const float effectLevel = effectLevelParam->load();

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
            const auto& s = settings[(size_t) b];
            auto& band = bands[(size_t) b];

            bool enabled = s.enabled;

            float bandInL = isSeries ? stageL : dryL;
            float bandInR = isSeries ? stageR : dryR;

            // Muted bands still advance and decay; re-enabling must not replay frozen audio.
            if (! enabled) bandInL = bandInR = 0.0f;

            // The speed/depth curves remain approximations, not measured Yamaha curves.
            band.lfoPhase += s.phaseIncrement;
            if (band.lfoPhase >= 1.0) band.lfoPhase -= 1.0;
            const float modMs = computeLfoValue (band.lfoPhase, s.wave) * s.modulationMs;
            const float totalTimeMs = juce::jlimit (0.0f, kMaxBandTimeMs + kMaxModMs, s.baseTimeMs + modMs);
            // A causal feedback line needs at least one sample. Reading the current
            // write cell at zero delay instead returns an entire old buffer cycle.
            float delaySamples = juce::jmax (1.0f, (float) (totalTimeMs / 1000.0 * currentSampleRate));

            float readPosF = (float) band.writePos - delaySamples;
            float readL = readInterpolated (band.bufL, band.bufLen, readPosF);
            float readR = readInterpolated (band.bufR, band.bufLen, readPosF);

            float feedback = s.feedback;
            float fbL = readL, fbR = readR;
            if (band.lastLowCut >= 0.02f)
            {
                fbL = band.loopLowCutL.processSample (fbL);
                fbR = band.loopLowCutR.processSample (fbR);
            }
            if (band.lastHighCut >= 0.02f)
            {
                fbL = band.loopHighCutL.processSample (fbL);
                fbR = band.loopHighCutR.processSample (fbR);
            }
            fbL *= feedback; fbR *= feedback;

            band.bufL[(size_t) band.writePos] = bandInL + fbL;
            band.bufR[(size_t) band.writePos] = bandInR + fbR;
            band.writePos = (band.writePos + 1) % band.bufLen;

            float bandOutL = readL * s.gainL;
            float bandOutR = readR * s.gainR;

            if (! enabled) continue;

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

        float wetL = (isSeries ? (anyEnabled ? stageL : 0.0f) : wetSumL) * effectLevel;
        float wetR = (isSeries ? (anyEnabled ? stageR : 0.0f) : wetSumR) * effectLevel;

        float finalL = (dryL * directPanL * directLevel + wetL) * outGain;
        float finalR = (dryR * directPanR * directLevel + wetR) * outGain;

        outL[n] = isBypassed ? dryL : finalL;
        outR[n] = isBypassed ? dryR : finalR;
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
    const juce::ScopedLock lock (patchLock);
    if (index < 0 || index >= kTotalPatches) return {};
    return patchNames[index];
}

void GrooveTapDelayAudioProcessor::loadPatch (int index)
{
    const juce::ScopedLock lock (patchLock);
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
    const juce::ScopedLock lock (patchLock);
    if (! isPatchEditable (index)) return; // Factory bank is read-only
    index = juce::jlimit (0, kTotalPatches - 1, index);
    patchXmlStore.set (index, apvts.copyState().createXml()->toString());
    patchNames.set (index, newName.isNotEmpty() ? newName : patchNames[index]);
}

void GrooveTapDelayAudioProcessor::renamePatch (int index, const juce::String& newName)
{
    const juce::ScopedLock lock (patchLock);
    if (! isPatchEditable (index) || newName.isEmpty()) return;
    patchNames.set (juce::jlimit (0, kTotalPatches - 1, index), newName);
}

void GrooveTapDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock (patchLock);
    juce::XmlElement root ("GrooveTapState");
    root.setAttribute ("currentPatch", currentPatchIndex.load());

    if (auto paramsXml = apvts.copyState().createXml())
        root.addChildElement (paramsXml.release());

    auto* bank = new juce::XmlElement ("PatchBank");
    for (int i = kNumFactorySlots; i < kTotalPatches; ++i)
    {
        juce::String initialName = i < kNumFactorySlots + kNumUserSlots
            ? juce::String (factoryPresets[(size_t) (i - kNumFactorySlots)].name)
            : "Init " + juce::String (i - (kNumFactorySlots + kNumUserSlots) + 1);
        bool nameChanged = patchNames[i] != initialName;
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
    const juce::ScopedLock lock (patchLock);
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;

    if (xml->hasTagName ("GrooveTapState"))
    {
        currentPatchIndex = juce::jlimit (0, kTotalPatches - 1, xml->getIntAttribute ("currentPatch", 0));
        // A new session state replaces the bank, including previously unsaved slots.
        for (int i = kNumFactorySlots; i < kTotalPatches; ++i)
        {
            patchXmlStore.set (i, {});
            patchNames.set (i, i < kNumFactorySlots + kNumUserSlots
                ? juce::String (factoryPresets[(size_t) (i - kNumFactorySlots)].name)
                : "Init " + juce::String (i - (kNumFactorySlots + kNumUserSlots) + 1));
        }

        if (auto* paramsXml = xml->getChildByName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*paramsXml));

        if (auto* bank = xml->getChildByName ("PatchBank"))
        {
            for (auto* slot : bank->getChildIterator())
            {
                int idx = slot->getIntAttribute ("index", -1);
                if (! isPatchEditable (idx)) continue;
                patchNames.set (idx, slot->getStringAttribute ("name"));
                if (auto* inner = slot->getChildByName (apvts.state.getType()))
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
