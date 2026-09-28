#include "PluginProcessor.h"
#include "PluginEditor.h"

GrooveTapDelayAudioProcessorEditor::GrooveTapDelayAudioProcessorEditor (GrooveTapDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // ---- Patch / preset bar ----
    addAndMakeVisible (prevPatchButton);
    prevPatchButton.onClick = [this] { goToPatch (proc.getCurrentPatchIndex() - 1); };

    addAndMakeVisible (nextPatchButton);
    nextPatchButton.onClick = [this] { goToPatch (proc.getCurrentPatchIndex() + 1); };

    addAndMakeVisible (patchNumberLabel);
    patchNumberLabel.setJustificationType (juce::Justification::centred);
    patchNumberLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff17181d));
    patchNumberLabel.setEditable (false, true, false);
    patchNumberLabel.onTextChange = [this]
    {
        auto n = patchNumberLabel.getText().getIntValue();
        if (n >= 1 && n <= proc.getNumPatches())
            goToPatch (n - 1);
        else
            refreshPatchDisplay();
    };

    addAndMakeVisible (patchNameLabel);
    patchNameLabel.setJustificationType (juce::Justification::centredLeft);
    patchNameLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    patchNameLabel.onTextChange = [this]
    {
        if (proc.isPatchEditable (proc.getCurrentPatchIndex()))
            proc.renamePatch (proc.getCurrentPatchIndex(), patchNameLabel.getText());
        refreshPatchDisplay();
    };

    addAndMakeVisible (browsePatchButton);
    browsePatchButton.onClick = [this] { showPatchBrowser(); };

    addAndMakeVisible (savePatchButton);
    savePatchButton.onClick = [this]
    {
        auto idx = proc.getCurrentPatchIndex();
        if (proc.isPatchEditable (idx))
        {
            proc.savePatch (idx, patchNameLabel.getText());
            refreshPatchDisplay();
        }
    };

    // ---- EDIT FUNCTION cluster (top right): GROUP / SYNC / WAVE / BYPASS ----
    setupGroupLabel (editFunctionHeading, "EDIT FUNCTION", true);
    addAndMakeVisible (editFunctionHeading);

    addAndMakeVisible (groupBox);
    groupBox.addItemList (GrooveTapDelayAudioProcessor::topologyChoices, 1);
    setupGroupLabel (groupLabel, "GROUP");
    groupLabel.attachToComponent (&groupBox, false);
    addAndMakeVisible (groupLabel);
    groupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "topology", groupBox);

    addAndMakeVisible (syncBox);
    syncBox.addItemList (GrooveTapDelayAudioProcessor::clockChoices, 1);
    setupGroupLabel (syncLabel, "SYNC");
    syncLabel.attachToComponent (&syncBox, false);
    addAndMakeVisible (syncLabel);
    syncBox.onChange = [this] { timeKnob.setEnabled (syncBox.getSelectedItemIndex() <= 0); };

    addAndMakeVisible (waveBox);
    waveBox.addItemList (GrooveTapDelayAudioProcessor::waveChoices, 1);
    setupGroupLabel (waveLabel, "WAVE");
    waveLabel.attachToComponent (&waveBox, false);
    addAndMakeVisible (waveLabel);

    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "bypass", bypassButton);

    // ---- EFFECT BAND select strip (top left) ----
    setupGroupLabel (effectBandHeading, "EFFECT BAND", true);
    addAndMakeVisible (effectBandHeading);

    for (int b = 0; b < kNumBands; ++b)
    {
        auto btn = std::make_unique<juce::TextButton> (juce::String (b + 1));
        btn->onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (*btn);
        bandButtons[(size_t) b] = std::move (btn);
    }

    // ---- Band title / on-off / tap tempo ----
    addAndMakeVisible (bandTitleLabel);
    bandTitleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    bandTitleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (bandEnabledButton);

    addAndMakeVisible (tapTempoButton);
    tapTempoButton.onClick = [this]
    {
        auto now = juce::Time::currentTimeMillis();
        if (lastTapTimeMs != 0)
        {
            auto intervalMs = (float) (now - lastTapTimeMs);
            if (intervalMs > 50.0f && intervalMs < 3000.0f)
            {
                auto id = GrooveTapDelayAudioProcessor::bandParamId (selectedBand, "time");
                if (auto* param = proc.apvts.getParameter (id))
                {
                    float clamped = juce::jlimit (0.0f, kMaxBandTimeMs, intervalMs);
                    param->setValueNotifyingHost (param->convertTo0to1 (clamped));
                }
            }
        }
        lastTapTimeMs = now;
    };

    // ---- Top knob row: TAP(Sync%) / SPEED / DEPTH / PAN / LEVEL ----
    setupKnob (tapKnob, tapLabel, "Tap (Sync %)");
    setupKnob (speedKnob, speedLabel, "Speed");
    setupKnob (depthKnob, depthLabel, "Depth");
    setupKnob (panKnob, panLabel, "Pan");
    setupKnob (levelKnob, levelLabel, "Level");

    // ---- Bottom knob row: TIME / LOW CUT / HIGH CUT / FEEDBACK ----
    setupKnob (timeKnob, timeLabel, "Time");
    setupKnob (lowCutKnob, lowCutLabel, "Low Cut");
    setupKnob (highCutKnob, highCutLabel, "High Cut");
    setupKnob (feedbackKnob, feedbackLabel, "Feedback");

    // ---- Global output cluster: EFFECT LEVEL / DIRECT LEVEL / DIRECT PAN / OUTPUT ----
    setupKnob (effectLevelKnob, effectLevelLabel, "Effect Level");
    effectLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "effectLevel", effectLevelKnob);

    setupKnob (directLevelKnob, directLevelLabel, "Direct Level");
    directLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "directLevel", directLevelKnob);

    setupKnob (directPanKnob, directPanLabel, "Direct Pan");
    directPanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "directPan", directPanKnob);

    setupKnob (outputKnob, outputLabel, "Output");
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "output", outputKnob);

    selectBand (0);
    refreshPatchDisplay();
    startTimerHz (8);

    setResizable (true, true);
    setResizeLimits (780, 490, 1400, 920);
    setSize (900, 590);
}

GrooveTapDelayAudioProcessorEditor::~GrooveTapDelayAudioProcessorEditor() { stopTimer(); }

void GrooveTapDelayAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    addAndMakeVisible (s);
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.0f));
    l.setJustificationType (juce::Justification::centred);
    l.attachToComponent (&s, false);
    addAndMakeVisible (l);
}

void GrooveTapDelayAudioProcessorEditor::setupGroupLabel (juce::Label& l, const juce::String& text, bool bold)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (11.0f, bold ? juce::Font::bold : juce::Font::plain));
    l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    l.setJustificationType (juce::Justification::centredLeft);
}

void GrooveTapDelayAudioProcessorEditor::refreshPatchDisplay()
{
    auto idx = proc.getCurrentPatchIndex();
    patchNumberLabel.setText (juce::String (idx + 1).paddedLeft ('0', 3), juce::dontSendNotification);
    patchNameLabel.setText (proc.getPatchName (idx), juce::dontSendNotification);

    bool editable = proc.isPatchEditable (idx);
    patchNameLabel.setEditable (false, editable, false);
    patchNameLabel.setColour (juce::Label::textColourId, editable ? juce::Colours::white : juce::Colour (0xffbbbbbb));
    savePatchButton.setEnabled (editable);

    juce::String bank = idx < kNumFactorySlots ? "Factory"
                       : idx < kNumFactorySlots + kNumUserSlots ? "User" : "User (extra)";
    patchNameLabel.setTooltip (bank + (editable ? " - double-click name to rename, Save to store current settings"
                                                 : " - read only"));
}

void GrooveTapDelayAudioProcessorEditor::goToPatch (int index)
{
    auto n = proc.getNumPatches();
    index = ((index % n) + n) % n; // wrap both directions
    proc.loadPatch (index);
    refreshPatchDisplay();
    // Existing per-band slider/combo attachments auto-refresh from the new
    // parameter values via their own listener callbacks - no rebind needed.
}

void GrooveTapDelayAudioProcessorEditor::showPatchBrowser()
{
    juce::PopupMenu menu;

    juce::PopupMenu factoryMenu;
    for (int i = 0; i < kNumFactorySlots; ++i)
        factoryMenu.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 3) + "  " + proc.getPatchName (i));
    menu.addSubMenu ("Factory 001-090", factoryMenu);

    juce::PopupMenu userMenu;
    for (int i = kNumFactorySlots; i < kNumFactorySlots + kNumUserSlots; ++i)
        userMenu.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 3) + "  " + proc.getPatchName (i));
    menu.addSubMenu ("User 091-180", userMenu);

    menu.addSeparator();
    menu.addItem (-1, "Extra User slots 181-999: type the number in the box, or use < / >", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (browsePatchButton),
        [this] (int result)
        {
            if (result >= 1)
                goToPatch (result - 1);
        });
}

void GrooveTapDelayAudioProcessorEditor::selectBand (int bandIndex)
{
    selectedBand = juce::jlimit (0, kNumBands - 1, bandIndex);
    bandTitleLabel.setText ("Effect Band " + juce::String (selectedBand + 1), juce::dontSendNotification);

    // Detach every old per-band attachment BEFORE creating any new one - see
    // the long comment this used to carry: creating a new attachment on a
    // shared widget immediately pushes the new band's value into that widget,
    // and if the OLD attachment is still listening at that instant it will
    // read that programmatic change as user input and write it back into the
    // PREVIOUS band's parameter. Resetting first removes the old listener
    // before any value gets pushed, so bands can no longer cross-talk.
    bandEnabledAttachment.reset();
    tapAttachment.reset();
    speedAttachment.reset();
    depthAttachment.reset();
    panAttachment.reset();
    levelAttachment.reset();
    timeAttachment.reset();
    lowCutAttachment.reset();
    highCutAttachment.reset();
    feedbackAttachment.reset();
    syncAttachment.reset();
    waveAttachment.reset();

    auto& a = proc.apvts;
    auto id = [this] (const char* suffix) { return GrooveTapDelayAudioProcessor::bandParamId (selectedBand, suffix); };

    bandEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, id ("enabled"), bandEnabledButton);
    tapAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("syncpct"), tapKnob);
    speedAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("lfospeed"), speedKnob);
    depthAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("lfodepth"), depthKnob);
    panAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("pan"), panKnob);
    levelAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("level"), levelKnob);
    timeAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("time"), timeKnob);
    lowCutAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("lowcut"), lowCutKnob);
    highCutAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("highcut"), highCutKnob);
    feedbackAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, id ("feedback"), feedbackKnob);
    syncAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, id ("clock"), syncBox);
    waveAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, id ("lfowave"), waveBox);

    timeKnob.setEnabled (syncBox.getSelectedItemIndex() <= 0);

    for (int b = 0; b < kNumBands; ++b)
        bandButtons[(size_t) b]->setToggleState (b == selectedBand, juce::dontSendNotification);

    repaint();
}

void GrooveTapDelayAudioProcessorEditor::timerCallback()
{
    for (int b = 0; b < kNumBands; ++b)
    {
        auto id = GrooveTapDelayAudioProcessor::bandParamId (b, "enabled");
        bool on = false;
        if (auto* param = proc.apvts.getParameter (id))
            on = param->getValue() > 0.5f;

        auto colour = on ? juce::Colour (0xff4caf50) : juce::Colour (0xff3a3a42);
        bandButtons[(size_t) b]->setColour (juce::TextButton::buttonColourId, colour);
    }
}

void GrooveTapDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff20232a));

    auto header = getLocalBounds().removeFromTop (30).reduced (14, 4);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawFittedText ("GrooveTap Delay", header.removeFromLeft (header.getWidth() / 2),
                       juce::Justification::centredLeft, 1);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colours::lightgrey);
    g.drawFittedText ("8-Band Rhythmic / Modulation Delay", header,
                       juce::Justification::centredRight, 1);

    // Panel background behind the band-editing area.
    auto panel = getLocalBounds();
    panel.removeFromTop (30 + 30 + 92 + 30); // header + patch bar + band/edit-function row + band-title row
    g.setColour (juce::Colour (0xff2b2e36));
    g.fillRoundedRectangle (panel.reduced (10, 4).toFloat(), 6.0f);

    // Divider between per-band knobs and the global output cluster.
    g.setColour (juce::Colour (0xff44454e));
    g.drawVerticalLine (globalClusterDividerX, (float) panel.getY() + 10.0f, (float) panel.getBottom() - 10.0f);
}

void GrooveTapDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    area.removeFromTop (30); // header text (painted, not a component)

    // --- Row: patch / preset bar ---
    auto patchBar = area.removeFromTop (30).reduced (14, 3);
    prevPatchButton.setBounds (patchBar.removeFromLeft (28));
    patchBar.removeFromLeft (4);
    patchNumberLabel.setBounds (patchBar.removeFromLeft (44));
    patchBar.removeFromLeft (6);
    nextPatchButton.setBounds (patchBar.removeFromLeft (28));
    patchBar.removeFromLeft (10);
    savePatchButton.setBounds (patchBar.removeFromRight (64));
    patchBar.removeFromRight (6);
    browsePatchButton.setBounds (patchBar.removeFromRight (72));
    patchBar.removeFromRight (10);
    patchNameLabel.setBounds (patchBar);

    // --- Row: EFFECT BAND strip (left) + EDIT FUNCTION cluster (right) ---
    // Tall enough for: a section heading, a gap, the combo boxes' own
    // auto-positioned labels (GROUP/SYNC/WAVE sit directly above each box),
    // and the boxes themselves - none of these bands may overlap.
    auto topRow = area.removeFromTop (92).reduced (14, 4);

    auto bandArea = topRow.removeFromLeft (juce::jmax (360, topRow.getWidth() * 2 / 5));
    effectBandHeading.setBounds (bandArea.removeFromTop (16));
    bandArea.removeFromTop (4);
    auto bandBtnRow = bandArea.removeFromTop (34);
    int bandBtnW = juce::jmin (56, bandBtnRow.getWidth() / kNumBands);
    for (int b = 0; b < kNumBands; ++b)
        bandButtons[(size_t) b]->setBounds (bandBtnRow.removeFromLeft (bandBtnW).reduced (2, 0));

    topRow.removeFromLeft (20);
    editFunctionHeading.setBounds (topRow.removeFromTop (16));
    topRow.removeFromTop (6);
    auto editRow = topRow; // remaining height holds each combo's own label + the combo
    int editW = editRow.getWidth() / 4;
    auto placeEditCombo = [&editRow, editW] (juce::ComboBox& box)
    {
        auto slot = editRow.removeFromLeft (editW);
        slot.removeFromTop (16); // room for the attached GROUP/SYNC/WAVE label
        box.setBounds (slot.reduced (4, 2));
    };
    placeEditCombo (groupBox);
    placeEditCombo (syncBox);
    placeEditCombo (waveBox);
    editRow.removeFromTop (16);
    bypassButton.setBounds (editRow.reduced (4, 2));

    // --- Row: band title / on-off / tap tempo ---
    auto titleRow = area.removeFromTop (30).reduced (16, 2);
    bandTitleLabel.setBounds (titleRow.removeFromLeft (160));
    bandEnabledButton.setBounds (titleRow.removeFromLeft (60));
    tapTempoButton.setBounds (titleRow.removeFromRight (110));

    area.removeFromTop (6);

    // Reserve space so each knob's auto-positioned label (which sits directly
    // above the slider) never lands on top of the row above it.
    constexpr int kLabelGap = 20;

    // Split remaining area into two equal knob rows, and split those into a
    // per-band section (left) and the global output cluster (right).
    auto knobsArea = area.reduced (16, 4);
    auto topKnobRow = knobsArea.removeFromTop (knobsArea.getHeight() / 2);
    auto bottomKnobRow = knobsArea;

    int globalClusterW = juce::jmin (280, topKnobRow.getWidth() * 3 / 10);
    globalClusterDividerX = topKnobRow.getRight() - globalClusterW - 6;

    auto topPerBand = topKnobRow.withTrimmedRight (globalClusterW + 12);
    auto topGlobal = topKnobRow.removeFromRight (globalClusterW);

    auto bottomPerBand = bottomKnobRow.withTrimmedRight (globalClusterW + 12);

    topPerBand.removeFromTop (kLabelGap);
    int perBandKnobW = topPerBand.getWidth() / 5;
    auto placeIn = [] (juce::Rectangle<int>& row, juce::Slider& s, int w)
    {
        s.setBounds (row.removeFromLeft (w).reduced (6, 0));
    };
    placeIn (topPerBand, tapKnob, perBandKnobW);
    placeIn (topPerBand, speedKnob, perBandKnobW);
    placeIn (topPerBand, depthKnob, perBandKnobW);
    placeIn (topPerBand, panKnob, perBandKnobW);
    placeIn (topPerBand, levelKnob, perBandKnobW);

    bottomPerBand.removeFromTop (kLabelGap);
    int bottomKnobW = bottomPerBand.getWidth() / 4;
    placeIn (bottomPerBand, timeKnob, bottomKnobW);
    placeIn (bottomPerBand, lowCutKnob, bottomKnobW);
    placeIn (bottomPerBand, highCutKnob, bottomKnobW);
    placeIn (bottomPerBand, feedbackKnob, bottomKnobW);

    topGlobal.removeFromTop (kLabelGap);
    int globalKnobW = topGlobal.getWidth() / 2;
    auto globalTopLeft = topGlobal.removeFromLeft (globalKnobW);
    auto globalTopRight = topGlobal;
    placeIn (globalTopLeft, effectLevelKnob, globalTopLeft.getWidth());
    placeIn (globalTopRight, directLevelKnob, globalTopRight.getWidth());

    auto globalBottomRow = bottomKnobRow.removeFromRight (globalClusterW);
    globalBottomRow.removeFromTop (kLabelGap);
    auto globalBottomLeft = globalBottomRow.removeFromLeft (globalKnobW);
    auto globalBottomRight = globalBottomRow;
    placeIn (globalBottomLeft, directPanKnob, globalBottomLeft.getWidth());
    placeIn (globalBottomRight, outputKnob, globalBottomRight.getWidth());
}
