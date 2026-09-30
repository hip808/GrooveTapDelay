#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "GrooveTapLookAndFeel.h"

// All the actual UI lives here, laid out at a fixed "native" resolution
// (see kNativeWidth/Height in the .cpp). The outer AudioProcessorEditor
// just scales this whole panel up/down via a transform to fit whatever
// window size the user resizes to - that's what lets the window shrink to
// a fraction of its native size without every knob/label needing its own
// tiny-size layout logic.
class GrooveTapContentPanel : public juce::Component,
                               private juce::Timer
{
public:
    explicit GrooveTapContentPanel (GrooveTapDelayAudioProcessor&);
    ~GrooveTapContentPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    GrooveTapDelayAudioProcessor& proc;

    // --- Patch / preset bar ---
    juce::TextButton prevPatchButton { "<" }, nextPatchButton { ">" }, browsePatchButton { "..." }, savePatchButton { "Sv" };
    juce::Label patchNumberLabel; // editable "###" - type a number + Enter to jump
    juce::Label patchNameLabel;   // editable when the current slot is a User slot

    void refreshPatchDisplay();
    void goToPatch (int index);
    void showPatchBrowser();

    // --- EDIT FUNCTION cluster (top right, mirrors the hardware's
    //     CONNECT/GROUP, SYNC and WAVE buttons) ---
    juce::Label editFunctionHeading;
    juce::ComboBox groupBox;   // topology: Parallel / Series
    juce::Label groupLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> groupAttachment;

    juce::ComboBox syncBox;    // per-band clock sync
    juce::Label syncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAttachment;

    juce::ComboBox waveBox;    // per-band LFO wave
    juce::Label waveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;

    juce::TextButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // ---- ASSIGN: a free "macro" knob that drives whichever global control
    // the target selector points at (mirrors the hardware's ASSIGNABLE
    // CONTROL) - a live pass-through, not a persisted DSP parameter. ----
    juce::Slider assignKnob;
    juce::Label assignLabel;
    juce::ComboBox assignTargetBox;
    juce::Label assignTargetLabel;
    void applyAssignToTarget();
    void syncAssignFromTarget();

    // --- EFFECT BAND select strip (top left, mirrors the hardware's 1-8 row) ---
    juce::Label effectBandHeading;
    std::array<std::unique_ptr<juce::TextButton>, kNumBands> bandButtons;
    int selectedBand = 0;

    // --- Per-band ON/OFF row (mirrors the hardware's own LED row - each
    //     band's active state, all 8 visible/toggleable at once, independent
    //     of which band is currently selected for editing) ---
    juce::Label bandOnOffHeading;
    std::array<std::unique_ptr<juce::TextButton>, kNumBands> bandOnOffButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, kNumBands> bandOnOffAttachments;

    // --- Band title / tap tempo ---
    juce::Label bandTitleLabel;
    juce::TextButton tapTempoButton { "TAP TEMPO" };
    juce::int64 lastTapTimeMs = 0;

    // --- Top knob row (hardware: TAP / SPEED / DEPTH / PAN / LEVEL) ---
    juce::Slider tapKnob, speedKnob, depthKnob, panKnob, levelKnob;
    juce::Label tapLabel, speedLabel, depthLabel, panLabel, levelLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tapAttachment, speedAttachment,
        depthAttachment, panAttachment, levelAttachment;

    // --- Bottom knob row (hardware: TIME [Coarse+Fine combined] / FILTER [Low+High Cut] / FEEDBACK) ---
    juce::Slider timeKnob, lowCutKnob, highCutKnob, feedbackKnob;
    juce::Label timeLabel, lowCutLabel, highCutLabel, feedbackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment, lowCutAttachment,
        highCutAttachment, feedbackAttachment;

    // --- Global output cluster (hardware: EFFECT LEVEL / DIRECT LEVEL / DIRECT PAN / OUTPUT) ---
    juce::Slider effectLevelKnob, directLevelKnob, directPanKnob, outputKnob;
    juce::Label effectLevelLabel, directLevelLabel, directPanLabel, outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> effectLevelAttachment,
        directLevelAttachment, directPanAttachment, outputAttachment;

    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text);
    void setupGroupLabel (juce::Label& l, const juce::String& text, bool bold = false);
    void selectBand (int bandIndex);
    void timerCallback() override;

    int globalClusterDividerX = 0;
    GrooveTapLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveTapContentPanel)
};

class GrooveTapDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GrooveTapDelayAudioProcessorEditor (GrooveTapDelayAudioProcessor&);
    ~GrooveTapDelayAudioProcessorEditor() override = default;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    GrooveTapContentPanel content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveTapDelayAudioProcessorEditor)
};
