#include "PluginProcessor.h"
#include "PluginEditor.h"

// The panel's fixed design resolution. The outer AudioProcessorEditor scales
// this whole panel (via a transform) to fit whatever window size the user
// resizes to, so this never needs to change for different window sizes.
static constexpr int kNativeWidth = 900;
static constexpr int kNativeHeight = 628;

GrooveTapContentPanel::GrooveTapContentPanel (GrooveTapDelayAudioProcessor& p)
    : proc (p)
{
    setLookAndFeel (&lookAndFeel);

    // ---- Patch / preset bar ----
    for (auto* b : { &prevPatchButton, &nextPatchButton, &browsePatchButton, &savePatchButton })
        GrooveTapLookAndFeel::setFootswitchStyle (*b);

    addAndMakeVisible (prevPatchButton);
    prevPatchButton.onClick = [this] { goToPatch (proc.getCurrentPatchIndex() - 1); };

    addAndMakeVisible (nextPatchButton);
    nextPatchButton.onClick = [this] { goToPatch (proc.getCurrentPatchIndex() + 1); };

    addAndMakeVisible (patchNumberLabel);
    patchNumberLabel.setJustificationType (juce::Justification::centred);
    patchNumberLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold));
    patchNumberLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffb454));
    patchNumberLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0a0a0a));
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
    patchNameLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold));
    patchNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffb454));
    patchNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0a0a0a));
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

    // ---- EDIT SETTINGS cluster (top right): GROUP / SYNC / WAVE / BYPASS ----
    setupGroupLabel (editFunctionHeading, "EDIT SETTINGS", true);
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

    GrooveTapLookAndFeel::setFootswitchStyle (bypassButton);
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "bypass", bypassButton);

    // ---- ASSIGN macro knob ----
    setupKnob (assignKnob, assignLabel, "Assign");
    assignKnob.setRange (0.0, 1.0);
    assignKnob.onValueChange = [this] { applyAssignToTarget(); };

    addAndMakeVisible (assignTargetBox);
    assignTargetBox.addItemList ({ "Output", "Effect Level", "Direct Level", "Direct Pan" }, 1);
    assignTargetBox.setSelectedItemIndex (0, juce::dontSendNotification);
    setupGroupLabel (assignTargetLabel, "ASSIGN TO");
    assignTargetLabel.attachToComponent (&assignTargetBox, false);
    addAndMakeVisible (assignTargetLabel);
    assignTargetBox.onChange = [this] { syncAssignFromTarget(); };

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

    // ---- Per-band ON/OFF row: always visible, independent of selection ----
    setupGroupLabel (bandOnOffHeading, "ON/OFF", true);
    addAndMakeVisible (bandOnOffHeading);

    for (int b = 0; b < kNumBands; ++b)
    {
        auto btn = std::make_unique<juce::TextButton> (juce::String (b + 1));
        btn->setClickingTogglesState (true);
        addAndMakeVisible (*btn);
        bandOnOffAttachments[(size_t) b] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, GrooveTapDelayAudioProcessor::bandParamId (b, "enabled"), *btn);
        bandOnOffButtons[(size_t) b] = std::move (btn);
    }

    // ---- Band title / tap tempo ----
    addAndMakeVisible (bandTitleLabel);
    bandTitleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    bandTitleLabel.setJustificationType (juce::Justification::centredLeft);

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
    syncAssignFromTarget();
    startTimerHz (8);

    setSize (kNativeWidth, kNativeHeight); // fixed native layout size - the outer
                                            // editor window scales this panel visually
}

GrooveTapContentPanel::~GrooveTapContentPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GrooveTapContentPanel::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text)
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

void GrooveTapContentPanel::setupGroupLabel (juce::Label& l, const juce::String& text, bool bold)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (11.0f, bold ? juce::Font::bold : juce::Font::plain));
    l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    l.setJustificationType (juce::Justification::centredLeft);
}

void GrooveTapContentPanel::refreshPatchDisplay()
{
    auto idx = proc.getCurrentPatchIndex();
    patchNumberLabel.setText (juce::String (idx + 1).paddedLeft ('0', 3), juce::dontSendNotification);
    patchNameLabel.setText (proc.getPatchName (idx), juce::dontSendNotification);

    bool editable = proc.isPatchEditable (idx);
    patchNameLabel.setEditable (false, editable, false);
    patchNameLabel.setColour (juce::Label::textColourId, editable ? juce::Colour (0xffffb454) : juce::Colour (0xffb37a2e));
    savePatchButton.setEnabled (editable);

    juce::String bank = idx < kNumFactorySlots ? "Factory"
                       : idx < kNumFactorySlots + kNumUserSlots ? "User" : "User (extra)";
    patchNameLabel.setTooltip (bank + (editable ? " - double-click name to rename, Save to store current settings"
                                                 : " - read only"));
}

void GrooveTapContentPanel::goToPatch (int index)
{
    auto n = proc.getNumPatches();
    index = ((index % n) + n) % n; // wrap both directions
    proc.loadPatch (index);
    refreshPatchDisplay();
    // Existing per-band slider/combo attachments auto-refresh from the new
    // parameter values via their own listener callbacks - no rebind needed.
}

void GrooveTapContentPanel::showPatchBrowser()
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

void GrooveTapContentPanel::selectBand (int bandIndex)
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

void GrooveTapContentPanel::timerCallback()
{
    // Keep the EFFECT BAND select strip's highlight in sync with which band
    // is currently being edited - a cheap, idempotent refresh.
    for (int b = 0; b < kNumBands; ++b)
    {
        bool selected = (b == selectedBand);
        auto colour = selected ? juce::Colour (0xff5dcaa5) : juce::Colour (0xff17302d);
        bandButtons[(size_t) b]->setColour (juce::TextButton::buttonColourId, colour);
        bandButtons[(size_t) b]->setColour (juce::TextButton::textColourOffId,
            selected ? juce::Colour (0xff073028) : juce::Colour (0xffcfd8d4));

        // Show literal ON/OFF text on the always-visible per-band row.
        bandOnOffButtons[(size_t) b]->setButtonText (bandOnOffButtons[(size_t) b]->getToggleState() ? "ON" : "OFF");
    }

    // If a MIDI Select-Band footswitch message moved the processor's own
    // band pointer, follow it in the UI too.
    int midiBand = proc.getMidiSelectedBand();
    if (midiBand != selectedBand)
        selectBand (midiBand);
}

void GrooveTapContentPanel::applyAssignToTarget()
{
    static const char* ids[] = { "output", "effectLevel", "directLevel", "directPan" };
    int idx = assignTargetBox.getSelectedItemIndex();
    if (idx < 0) return;
    if (auto* p = proc.apvts.getParameter (ids[idx]))
        p->setValueNotifyingHost ((float) assignKnob.getValue());
}

void GrooveTapContentPanel::syncAssignFromTarget()
{
    static const char* ids[] = { "output", "effectLevel", "directLevel", "directPan" };
    int idx = assignTargetBox.getSelectedItemIndex();
    if (idx < 0) return;
    if (auto* p = proc.apvts.getParameter (ids[idx]))
        assignKnob.setValue (p->getValue(), juce::dontSendNotification);
}

bool GrooveTapContentPanel::keyPressed (const juce::KeyPress& key)
{
    auto c = key.getTextCharacter();
    if (c >= '1' && c <= '8') { selectBand (c - '1'); return true; }
    if (c == 't' || c == 'T') { tapTempoButton.triggerClick(); return true; }
    if (c == 'b' || c == 'B') { bypassButton.triggerClick(); return true; }

    auto code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey || code == '[') { goToPatch (proc.getCurrentPatchIndex() - 1); return true; }
    if (code == juce::KeyPress::rightKey || code == ']') { goToPatch (proc.getCurrentPatchIndex() + 1); return true; }

    return false;
}

void GrooveTapContentPanel::paint (juce::Graphics& g)
{
    auto full = getLocalBounds();

    static const juce::Image panelBg = juce::ImageCache::getFromMemory (
        BinaryData::PanelBackground_png, BinaryData::PanelBackground_pngSize);

    if (panelBg.isValid())
        g.drawImage (panelBg, full.toFloat(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll (juce::Colour (0xff1f3230));

    auto header = getLocalBounds().removeFromTop (30).reduced (14, 4);
    g.setColour (juce::Colour (0xfff0e6c8));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold | juce::Font::italic));
    g.drawFittedText ("MOKU AI", header.removeFromLeft (header.getWidth() / 2),
                       juce::Justification::centredLeft, 1);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xffcfd8d4));
    g.drawFittedText ("8TAP - 8-BAND MODULATION DELAY", header,
                       juce::Justification::centredRight, 1);

    // A light translucent panel over the knob-editing area for contrast,
    // letting the faceplate texture still show through.
    auto panel = getLocalBounds();
    panel.removeFromTop (panelTopY); // computed in resized(), see its declaration
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (panel.reduced (10, 4).toFloat(), 6.0f);
    g.setColour (juce::Colour (0xfff0e6c8).withAlpha (0.35f));
    g.drawRoundedRectangle (panel.reduced (10, 4).toFloat(), 6.0f, 1.0f);

    // Divider between per-band knobs and the global output cluster.
    g.setColour (juce::Colour (0xfff0e6c8).withAlpha (0.35f));
    g.drawVerticalLine (globalClusterDividerX, (float) panel.getY() + 10.0f, (float) panel.getBottom() - 10.0f);
}

void GrooveTapContentPanel::resized()
{
    auto area = getLocalBounds();

    area.removeFromTop (30); // header text (painted, not a component)

    // --- Row: patch / preset bar (small round footswitch-style nav buttons
    //     flanking an amber LCD-style number + name readout) ---
    auto patchBar = area.removeFromTop (34).reduced (14, 1);
    prevPatchButton.setBounds (patchBar.removeFromLeft (30).withSizeKeepingCentre (28, 28));
    patchBar.removeFromLeft (4);
    patchNumberLabel.setBounds (patchBar.removeFromLeft (48));
    patchBar.removeFromLeft (6);
    nextPatchButton.setBounds (patchBar.removeFromLeft (30).withSizeKeepingCentre (28, 28));
    patchBar.removeFromLeft (10);
    savePatchButton.setBounds (patchBar.removeFromRight (32).withSizeKeepingCentre (28, 28));
    patchBar.removeFromRight (8);
    browsePatchButton.setBounds (patchBar.removeFromRight (32).withSizeKeepingCentre (28, 28));
    patchBar.removeFromRight (10);
    patchNameLabel.setBounds (patchBar);

    // --- Row: EFFECT BAND strip + ON/OFF row (left) + EDIT FUNCTION cluster (right) ---
    // Tall enough for: a section heading, a gap, the combo boxes' own
    // auto-positioned labels (GROUP/SYNC/WAVE sit directly above each box),
    // and the boxes themselves - none of these bands may overlap.
    auto topRow = area.removeFromTop (130).reduced (14, 4);

    auto bandArea = topRow.removeFromLeft (juce::jmax (360, topRow.getWidth() * 2 / 5));
    effectBandHeading.setBounds (bandArea.removeFromTop (16));
    bandArea.removeFromTop (4);
    auto bandBtnRow = bandArea.removeFromTop (34);
    int bandBtnW = juce::jmin (56, bandBtnRow.getWidth() / kNumBands);
    for (int b = 0; b < kNumBands; ++b)
        bandButtons[(size_t) b]->setBounds (bandBtnRow.removeFromLeft (bandBtnW).reduced (2, 0));

    bandArea.removeFromTop (8);
    bandOnOffHeading.setBounds (bandArea.removeFromTop (14));
    bandArea.removeFromTop (2);
    auto onOffBtnRow = bandArea.removeFromTop (28);
    int onOffBtnW = juce::jmin (56, onOffBtnRow.getWidth() / kNumBands);
    for (int b = 0; b < kNumBands; ++b)
        bandOnOffButtons[(size_t) b]->setBounds (onOffBtnRow.removeFromLeft (onOffBtnW).reduced (2, 0));

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
    assignTargetBox.setBounds (editRow.reduced (4, 2));

    // --- Row: band title / tap tempo / bypass ---
    auto titleRow = area.removeFromTop (30).reduced (16, 2);
    bandTitleLabel.setBounds (titleRow.removeFromLeft (200));
    bypassButton.setBounds (titleRow.removeFromRight (34).withSizeKeepingCentre (30, 30));
    titleRow.removeFromRight (10);
    tapTempoButton.setBounds (titleRow.removeFromRight (100));

    area.removeFromTop (6);
    panelTopY = area.getY(); // exactly where the translucent panel overlay should start

    // Reserve space so each knob's auto-positioned label (which sits directly
    // above the slider) never lands on top of the row above it.
    constexpr int kLabelGap = 20;

    // Split remaining area into two equal knob rows (6 knobs each, matching
    // the hardware's own TAP/SPEED/DEPTH/PAN/LEVEL/EFFECT-LEVEL and
    // TIME/LOW-CUT/HIGH-CUT/FEEDBACK/DIRECT-PAN/DIRECT-LEVEL rows), plus a
    // separate right-hand strip for OUTPUT and the ASSIGN macro knob.
    auto knobsArea = area.reduced (16, 4);
    auto topKnobRow = knobsArea.removeFromTop (knobsArea.getHeight() / 2);
    auto bottomKnobRow = knobsArea;

    int outputStripW = juce::jmin (140, topKnobRow.getWidth() / 6);
    globalClusterDividerX = topKnobRow.getRight() - outputStripW - 6;

    auto topPerBand = topKnobRow.withTrimmedRight (outputStripW + 12);
    auto bottomPerBand = bottomKnobRow.withTrimmedRight (outputStripW + 12);
    auto outputStripTop = topKnobRow.removeFromRight (outputStripW);
    auto outputStripBottom = bottomKnobRow.removeFromRight (outputStripW);

    auto placeIn = [] (juce::Rectangle<int>& row, juce::Slider& s, int w)
    {
        s.setBounds (row.removeFromLeft (w).reduced (6, 0));
    };

    topPerBand.removeFromTop (kLabelGap);
    int topKnobW = topPerBand.getWidth() / 6;
    placeIn (topPerBand, tapKnob, topKnobW);
    placeIn (topPerBand, speedKnob, topKnobW);
    placeIn (topPerBand, depthKnob, topKnobW);
    placeIn (topPerBand, panKnob, topKnobW);
    placeIn (topPerBand, levelKnob, topKnobW);
    placeIn (topPerBand, effectLevelKnob, topKnobW);

    bottomPerBand.removeFromTop (kLabelGap);
    int bottomKnobW = bottomPerBand.getWidth() / 6;
    placeIn (bottomPerBand, timeKnob, bottomKnobW);
    placeIn (bottomPerBand, lowCutKnob, bottomKnobW);
    placeIn (bottomPerBand, highCutKnob, bottomKnobW);
    placeIn (bottomPerBand, feedbackKnob, bottomKnobW);
    placeIn (bottomPerBand, directPanKnob, bottomKnobW);
    placeIn (bottomPerBand, directLevelKnob, bottomKnobW);

    outputStripTop.removeFromTop (kLabelGap);
    placeIn (outputStripTop, outputKnob, outputStripTop.getWidth());

    outputStripBottom.removeFromTop (kLabelGap);
    placeIn (outputStripBottom, assignKnob, outputStripBottom.getWidth());
}

//==============================================================================
// Outer AudioProcessorEditor: owns the resizable window and just scales the
// fixed-resolution GrooveTapContentPanel to fit it, letting the window shrink
// down to a fraction of its native size without any per-control tiny-layout
// logic (every control just gets visually smaller, uniformly, together).
GrooveTapDelayAudioProcessorEditor::GrooveTapDelayAudioProcessorEditor (GrooveTapDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (kNativeWidth / 3, kNativeHeight / 3, kNativeWidth * 2, kNativeHeight * 2);
    setSize (kNativeWidth, kNativeHeight);
}

void GrooveTapDelayAudioProcessorEditor::resized()
{
    float scale = juce::jmin ((float) getWidth() / (float) kNativeWidth,
                               (float) getHeight() / (float) kNativeHeight);

    float scaledW = kNativeWidth * scale;
    float scaledH = kNativeHeight * scale;
    float offsetX = ((float) getWidth() - scaledW) * 0.5f;
    float offsetY = ((float) getHeight() - scaledH) * 0.5f;

    content.setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
    content.setBounds (0, 0, kNativeWidth, kNativeHeight);
}

bool GrooveTapDelayAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    return content.keyPressed (key);
}
