#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

GrooveTapDelayAudioProcessorEditor::GrooveTapDelayAudioProcessorEditor (GrooveTapDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&stompLook);
    panelTexture = juce::ImageCache::getFromMemory (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);
    for (auto* foot : { &selectFoot, &bandEnabledButton, &tapTempoButton, &bypassButton })
        foot->getProperties().set ("foot", true);
    bypassButton.setClickingTogglesState (true);
    bandEnabledButton.setClickingTogglesState (true);
    addAndMakeVisible (tempoReadout);
    tempoReadout.setText ("-- BPM", juce::dontSendNotification);
    tempoReadout.setColour (juce::Label::textColourId, stompLook.amber);
    tempoReadout.setColour (juce::Label::backgroundColourId, juce::Colour (0xff07191a));
    tempoReadout.setJustificationType (juce::Justification::centred);
    tempoReadout.setTooltip ("Last accepted tap interval, limited to this band's 696 ms range");
    addAndMakeVisible (selectFoot);
    selectFoot.onClick = [this] { selectBand ((selectedBand + 1) % kNumBands); };
    setupKnob (assignKnob, assignLabel, "ASSIGN");
    addAndMakeVisible (assignTarget);
    assignTarget.addItemList ({ "Time", "Feedback", "Depth", "Speed", "Level", "Tap", "Pan" }, 1);
    assignTarget.setSelectedId (3, juce::dontSendNotification);
    assignTarget.onChange = [this] { bindAssign(); };
    assignKnob.setTooltip ("Adjust the selected band's parameter chosen below this knob");
    // ---- Patch / preset bar ----
    addAndMakeVisible (prevPatchButton);
    prevPatchButton.onClick = [this] { goToPatch (proc.getCurrentPatchIndex() - 1); };

    addAndMakeVisible (nextPatchButton);
    nextPatchButton.onClick = [this] { goToPatch (proc.getCurrentPatchIndex() + 1); };

    addAndMakeVisible (patchNumberLabel);
    patchNumberLabel.setJustificationType (juce::Justification::centred);
    patchNumberLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
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
        else
        {
            juce::PopupMenu slots;
            for (int i = kNumFactorySlots; i < kNumFactorySlots + kNumUserSlots; ++i)
                slots.addItem (i + 1, juce::String (i + 1) + "  " + proc.getPatchName (i));
            slots.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (savePatchButton),
                [safe = juce::Component::SafePointer<GrooveTapDelayAudioProcessorEditor> (this)] (int result)
                {
                    if (safe != nullptr && result > 0)
                    {
                        auto name = safe->proc.getPatchName (safe->proc.getCurrentPatchIndex());
                        safe->proc.savePatch (result - 1, name);
                        safe->goToPatch (result - 1);
                    }
                });
        }
    };

    // ---- EDIT FUNCTION cluster (top right): GROUP / SYNC / WAVE / BYPASS ----
    setupGroupLabel (editFunctionHeading, "EDIT FUNCTION", true);
    addAndMakeVisible (editFunctionHeading);

    addAndMakeVisible (groupBox);
    groupBox.addItemList (GrooveTapDelayAudioProcessor::topologyChoices, 1);
    setupGroupLabel (groupLabel, "ROUTING");
    groupLabel.attachToComponent (&groupBox, false);
    addAndMakeVisible (groupLabel);
    groupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "topology", groupBox);

    addAndMakeVisible (syncBox);
    syncBox.addItemList (GrooveTapDelayAudioProcessor::clockChoices, 1);
    setupGroupLabel (syncLabel, "CLOCK");
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
        btn->getProperties().set ("selector", true);
        btn->onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (*btn);
        btn->setTooltip ("Select band " + juce::String (b + 1) + " for editing");
        bandButtons[(size_t) b] = std::move (btn);
        auto power = std::make_unique<juce::TextButton> ("ON");
        power->setClickingTogglesState (true);
        power->getProperties().set ("power", true);
        power->setTitle ("Band " + juce::String (b + 1) + " on/off");
        power->setTooltip ("Enable or disable band " + juce::String (b + 1) + " without changing selection");
        addAndMakeVisible (*power);
        bandPowerAttachments[(size_t) b] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
            (proc.apvts, GrooveTapDelayAudioProcessor::bandParamId (b, "enabled"), *power);
        bandPowerButtons[(size_t) b] = std::move (power);
    }

    // ---- Band title / on-off / tap tempo ----
    addAndMakeVisible (bandTitleLabel);
    bandTitleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    bandTitleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (bandEnabledButton);

    addAndMakeVisible (tapTempoButton);
    tapTempoButton.setTooltip ("Tap twice to set the selected band's Time (maximum 696 ms); unavailable with host clock sync");
    tapTempoButton.onClick = [this]
    {
        if (syncBox.getSelectedItemIndex() > 0) return;
        auto now = juce::Time::getMillisecondCounterHiRes();
        if (lastTapTimeMs > 0)
        {
            auto intervalMs = (float) (now - lastTapTimeMs);
            if (intervalMs > 50.0f && intervalMs < 3000.0f)
            {
                auto id = GrooveTapDelayAudioProcessor::bandParamId (selectedBand, "time");
                if (auto* param = proc.apvts.getParameter (id))
                {
                    float clamped = juce::jlimit (0.0f, kMaxBandTimeMs, intervalMs);
                    param->beginChangeGesture();
                    param->setValueNotifyingHost (param->convertTo0to1 (clamped));
                    param->endChangeGesture();
                    tempoReadout.setText (juce::String (60000.0f / clamped, 1) + " BPM", juce::dontSendNotification);
                }
            }
        }
        lastTapTimeMs = now;
    };

    // ---- Top knob row: TAP(Sync%) / SPEED / DEPTH / PAN / LEVEL ----
    setupKnob (tapKnob, tapLabel, "TAP");
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

    patchNumberLabel.getProperties().set ("lcd", true);
    patchNameLabel.getProperties().set ("lcd", true);
    tempoReadout.getProperties().set ("lcd", true);
    selectBand (0);
    refreshPatchDisplay();
    startTimerHz (8);

    setResizable (true, true);
    // Keep the full panel usable on smaller laptop screens.  The editor scales
    // from its 1200 x 800 design coordinates, so 420 x 280 is 35% of the
    // original dimensions while retaining the exact aspect ratio.
    setResizeLimits (420, 280, 1800, 1200);
    getConstrainer()->setFixedAspectRatio (1.5);
    setSize (1200, 800);
}

GrooveTapDelayAudioProcessorEditor::~GrooveTapDelayAudioProcessorEditor() { stopTimer(); setLookAndFeel (nullptr); }

void GrooveTapDelayAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);
    l.setText (text.toUpperCase(), juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.0f));
    l.setJustificationType (juce::Justification::centred);
    l.attachToComponent (&s, false);
    addAndMakeVisible (l);
}

void GrooveTapDelayAudioProcessorEditor::setupGroupLabel (juce::Label& l, const juce::String& text, bool bold)
{
    l.setText (text.toUpperCase(), juce::dontSendNotification);
    l.setFont (juce::FontOptions (11.0f, bold ? juce::Font::bold : juce::Font::plain));
    l.setColour (juce::Label::textColourId, stompLook.cream);
    l.setJustificationType (juce::Justification::centredLeft);
}

void GrooveTapDelayAudioProcessorEditor::refreshPatchDisplay()
{
    auto idx = proc.getCurrentPatchIndex();
    patchNumberLabel.setText (juce::String (idx + 1).paddedLeft ('0', 3), juce::dontSendNotification);
    patchNameLabel.setText (proc.getPatchName (idx), juce::dontSendNotification);

    bool editable = proc.isPatchEditable (idx);
    patchNameLabel.setEditable (false, editable, false);
    patchNameLabel.setColour (juce::Label::textColourId, stompLook.amber);
    savePatchButton.setEnabled (true);
    savePatchButton.setTooltip (editable ? "Save changes to this user slot" : "Save current settings to a user slot");

    juce::String bank = idx < kNumFactorySlots ? "Factory"
                       : idx < kNumFactorySlots + kNumUserSlots ? "User" : "User (extra)";
    patchNameLabel.setTooltip (bank + (editable ? " - double-click name to rename, Save to store current settings"
                                                 : " - read only"));
}

void GrooveTapDelayAudioProcessorEditor::goToPatch (int index)
{
    auto n = proc.getNumPatches();
    index = ((index % n) + n) % n; // wrap both directions
    lastTapTimeMs = 0;
    tempoReadout.setText ("-- BPM", juce::dontSendNotification);
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
        [safe = juce::Component::SafePointer<GrooveTapDelayAudioProcessorEditor> (this)] (int result)
        {
            if (safe != nullptr && result >= 1)
                safe->goToPatch (result - 1);
        });
}

void GrooveTapDelayAudioProcessorEditor::selectBand (int bandIndex)
{
    lastTapTimeMs = 0;
    tempoReadout.setText ("-- BPM", juce::dontSendNotification);
    selectedBand = juce::jlimit (0, kNumBands - 1, bandIndex);
    bandTitleLabel.setText ("Effect Band " + juce::String (selectedBand + 1), juce::dontSendNotification);

    // Detach every old per-band attachment BEFORE creating any new one - see
    // the long comment this used to carry: creating a new attachment on a
    // shared widget immediately pushes the new band's value into that widget,
    // and if the OLD attachment is still listening at that instant it will
    // read that programmatic change as user input and write it back into the
    // PREVIOUS band's parameter. Resetting first removes the old listener
    // before any value gets pushed, so bands can no longer cross-talk.
    assignAttachment.reset();
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

    bindAssign();
    repaint();
}

void GrooveTapDelayAudioProcessorEditor::bindAssign()
{
    assignAttachment.reset();
    static const char* suffixes[] = { "time", "feedback", "lfodepth", "lfospeed", "level", "syncpct", "pan" };
    auto index = juce::jlimit (0, 6, assignTarget.getSelectedItemIndex());
    assignAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (proc.apvts, GrooveTapDelayAudioProcessor::bandParamId (selectedBand, suffixes[index]), assignKnob);
    assignKnob.setEnabled (index != 0 || syncBox.getSelectedItemIndex() <= 0);
}

void GrooveTapDelayAudioProcessorEditor::timerCallback()
{
    tapTempoButton.setEnabled (syncBox.getSelectedItemIndex() <= 0);
    if (! patchNameLabel.isBeingEdited() && ! patchNumberLabel.isBeingEdited()) refreshPatchDisplay();
    assignKnob.setEnabled (assignTarget.getSelectedId() != 1 || syncBox.getSelectedItemIndex() <= 0);
}

void GrooveTapDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 1200.0f, (float) getHeight() / 800.0f));
    g.drawImage (panelTexture, juce::Rectangle<float> (0, 0, 1200, 800));
    auto screen = [&g] (float x, float y, float w, float h)
    {
        g.setColour (juce::Colour (0xff030505)); g.fillRoundedRectangle (x, y, w, h, 5);
        g.setColour (juce::Colour (0xff62706a)); g.drawRoundedRectangle (x, y, w, h, 5, 1);
        g.setColour (juce::Colours::black); g.fillRoundedRectangle (x+6, y+6, w-12, h-12, 3);
    };
    screen (55, 90, 251, 80);
    screen (779, 636, 108, 38);
    g.setColour (stompLook.cream);
    g.setFont (StompLookAndFeel::panelFont (15));
    g.drawText ("SELECT", 329, 94, 57, 25, juce::Justification::centredLeft);
    g.drawText ("ON/OFF", 329, 132, 57, 25, juce::Justification::centredLeft);
    g.drawVerticalLine (1038, 182, 454);
    // Draw the settings frame with a deliberate gap behind its heading.
    // A complete rounded rectangle used to strike through "EDIT SETTINGS".
    g.drawLine (836, 56, 927, 56, 1);
    g.drawLine (1080, 56, 1139, 56, 1);
    g.drawLine (836, 56, 836, 137, 1);
    g.drawLine (1139, 56, 1139, 137, 1);
    g.drawLine (836, 137, 1139, 137, 1);
    g.drawLine (374, 51, 534, 51); g.drawLine (658, 51, 804, 51);
    g.drawLine (374, 51, 374, 78); g.drawLine (804, 51, 804, 78);
}

void GrooveTapDelayAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / 1200.0f;
    auto place = [this] (juce::Component& c, int x, int y, int w, int h)
    {
        c.setBounds (juce::roundToInt (x * getWidth() / 1200.0), juce::roundToInt (y * getHeight() / 800.0),
                     juce::roundToInt (w * getWidth() / 1200.0), juce::roundToInt (h * getHeight() / 800.0));
    };
    place (patchNumberLabel, 76, 110, 65, 40);
    place (patchNameLabel, 146, 110, 151, 40);
    patchNumberLabel.setColour (juce::Label::textColourId, stompLook.amber);
    patchNumberLabel.setFont (juce::FontOptions ("Menlo", 32*scale, juce::Font::plain));
    patchNameLabel.setFont (juce::FontOptions ("Menlo", 29*scale, juce::Font::plain));
    place (prevPatchButton, 62, 182, 45, 57); prevPatchButton.setButtonText ("PREV");
    place (nextPatchButton, 127, 182, 45, 57); nextPatchButton.setButtonText ("NEXT");
    place (browsePatchButton, 188, 182, 49, 57); browsePatchButton.setButtonText ("BROWSE");
    place (savePatchButton, 254, 182, 45, 57); savePatchButton.setButtonText ("SAVE");
    for (auto* b : { &prevPatchButton, &nextPatchButton, &browsePatchButton, &savePatchButton })
        b->getProperties().set ("smallSwitch", true);
    place (effectBandHeading, 538, 38, 125, 27);
    effectBandHeading.setFont (StompLookAndFeel::panelFont (20*scale));
    for (int b = 0; b < kNumBands; ++b)
    {
        place (*bandButtons[(size_t) b], 388 + b * 53, 68, 39, 55);
        place (*bandPowerButtons[(size_t) b], 388 + b * 53, 127, 39, 39);
    }
    place (editFunctionHeading, 933, 44, 142, 24);
    editFunctionHeading.setText ("EDIT SETTINGS", juce::dontSendNotification);
    editFunctionHeading.setFont (StompLookAndFeel::panelFont (18*scale));
    place (groupBox, 850, 93, 99, 35);
    place (syncBox, 965, 93, 72, 35);
    place (waveBox, 1059, 93, 70, 35);
    bandTitleLabel.setVisible (false);
    auto knob = [&place, scale] (juce::Slider& s, int col, int y)
    {
        place (s, 338 + col * 117, y, 99, 105);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt (90*scale), juce::roundToInt (23*scale));
    };
    knob (tapKnob, 0, 204); knob (speedKnob, 1, 204); knob (depthKnob, 2, 204);
    knob (panKnob, 3, 204); knob (levelKnob, 4, 204); knob (effectLevelKnob, 5, 204);
    knob (timeKnob, 0, 350); knob (lowCutKnob, 1, 350); knob (highCutKnob, 2, 350);
    knob (feedbackKnob, 3, 350); knob (directPanKnob, 4, 350); knob (directLevelKnob, 5, 350);
    place (outputKnob, 1053, 319, 99, 116);
    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                juce::roundToInt (90*scale), juce::roundToInt (23*scale));
    place (assignKnob, 114, 304, 125, 128);
    assignKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    // Match the value typography and control height used by EDIT SETTINGS.
    place (assignTarget, 123, 437, 106, 35);
    for (auto* l : { &tapLabel, &speedLabel, &depthLabel, &panLabel, &levelLabel, &effectLevelLabel,
                     &timeLabel, &lowCutLabel, &highCutLabel, &feedbackLabel, &directPanLabel,
                     &directLevelLabel, &outputLabel, &assignLabel, &groupLabel, &syncLabel, &waveLabel })
        l->setFont (StompLookAndFeel::panelFont (16*scale));
    selectFoot.setButtonText ("SELECT BAND"); bandEnabledButton.setButtonText ("BAND ON/OFF");
    selectFoot.getProperties().set ("number", "1");
    bandEnabledButton.getProperties().set ("number", "2");
    tapTempoButton.getProperties().set ("number", "3");
    place (selectFoot, 99, 582, 155, 161);
    place (bandEnabledButton, 384, 582, 155, 161);
    place (tapTempoButton, 661, 582, 155, 161);
    place (bypassButton, 943, 582, 155, 161);
    place (tempoReadout, 783, 639, 100, 32);
    tempoReadout.setFont (juce::FontOptions ("Menlo", 22*scale, juce::Font::plain));
    tempoReadout.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}
