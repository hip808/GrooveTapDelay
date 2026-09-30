#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "StompLookAndFeel.h"

class GrooveTapDelayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit GrooveTapDelayAudioProcessorEditor (GrooveTapDelayAudioProcessor&);
    ~GrooveTapDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    StompLookAndFeel stompLook;
    GrooveTapDelayAudioProcessor& proc;
    juce::Image panelTexture;
    juce::Label tempoReadout;
    juce::TooltipWindow tooltips { this, 600 };
    juce::TextButton selectFoot { "1  SELECT BAND" };
    juce::Slider assignKnob;
    juce::Label assignLabel;
    juce::ComboBox assignTarget;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> assignAttachment;
    void bindAssign();
    std::array<std::unique_ptr<juce::TextButton>, kNumBands> bandPowerButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, kNumBands> bandPowerAttachments;

    // --- Patch / preset bar ---
    juce::TextButton prevPatchButton { "<" }, nextPatchButton { ">" }, browsePatchButton { "Browse" }, savePatchButton { "Save" };
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

    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // --- EFFECT BAND select strip (top left, mirrors the hardware's 1-8 row) ---
    juce::Label effectBandHeading;
    std::array<std::unique_ptr<juce::TextButton>, kNumBands> bandButtons;
    int selectedBand = 0;

    // --- Band title / on-off / tap tempo ---
    juce::Label bandTitleLabel;
    juce::TextButton bandEnabledButton { "2  BAND ON/OFF" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandEnabledAttachment;
    juce::TextButton tapTempoButton { "TAP TEMPO" };
    double lastTapTimeMs = 0;

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


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveTapDelayAudioProcessorEditor)
};
