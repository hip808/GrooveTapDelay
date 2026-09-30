#include "PluginProcessor.h"
#include "PluginEditor.h"

GrooveTapDelayAudioProcessorEditor::GrooveTapDelayAudioProcessorEditor (GrooveTapDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&stompLook);
    panelTexture = juce::Image (juce::Image::RGB, 1200, 800, false);
    juce::Random grain (808);
    for (int y = 0; y < 800; ++y)
        for (int x = 0; x < 1200; ++x)
        {
            auto v = grain.nextInt (17) - 8;
            panelTexture.setPixelAt (x, y, juce::Colour::fromRGB ((juce::uint8) (18 + v), (juce::uint8) (92 + v), (juce::uint8) (96 + v)));
        }
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

    selectBand (0);
    refreshPatchDisplay();
    startTimerHz (8);

    setResizable (true, true);
    setResizeLimits (1000, 667, 1800, 1200);
    getConstrainer()->setFixedAspectRatio (1.5);
    setSize (1200, 800);
}

GrooveTapDelayAudioProcessorEditor::~GrooveTapDelayAudioProcessorEditor() { stopTimer(); setLookAndFeel (nullptr); }

void GrooveTapDelayAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
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
    g.fillAll (juce::Colour (0xff141a1a));
    g.drawImageAt (panelTexture, 0, 0);
    g.setGradientFill ({juce::Colours::white.withAlpha (0.08f), 0, 0, juce::Colours::black.withAlpha (0.22f), 0, 800, false});
    g.fillAll();
    g.setColour (juce::Colour (0xff121c1c));
    g.drawRoundedRectangle (5, 5, 1190, 790, 15, 12);
    g.setColour (stompLook.cream.withAlpha (0.4f));
    g.drawRoundedRectangle (14, 14, 1172, 772, 10, 1);
    g.setColour (juce::Colour (0xff101717)); g.fillRect (8, 487, 1184, 22);
    g.setColour (juce::Colour (0xff72918c)); g.drawHorizontalLine (490, 14, 1186);
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (42, 94, 282, 68, 6);
    g.setColour (stompLook.cream.withAlpha (0.4f)); g.drawRoundedRectangle (42, 94, 282, 68, 6, 2);
    g.setColour (stompLook.cream);
    g.setFont (juce::FontOptions ("Arial", 44.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("MOKU AI", 45, 30, 285, 58, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions ("Arial", 46.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("MOKU AI UD Stomp", 45, 525, 830, 60, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (14, juce::Font::bold));
    g.drawText ("8-BAND MODULATION DELAY", 48, 585, 500, 24, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11, juce::Font::bold));
    g.drawText ("SELECT", 336, 89, 56, 30, juce::Justification::centredRight);
    g.drawText ("ON/OFF", 336, 133, 56, 30, juce::Justification::centredRight);
    g.setColour (stompLook.cream.withAlpha (0.4f)); g.drawVerticalLine (1048, 205, 464);
    for (auto pt : {juce::Point<float> (24, 26), {1176, 26}, {24, 466}, {1176, 466}, {24, 528}, {1176, 528}, {24, 775}, {1176, 775}})
    {
        g.setColour (juce::Colour (0xff0a1414)); g.fillEllipse (pt.x - 7, pt.y - 7, 14, 14);
        g.setColour (juce::Colour (0xff82948d)); g.drawEllipse (pt.x - 7, pt.y - 7, 14, 14, 1);
        g.drawLine (pt.x - 3, pt.y - 3, pt.x + 3, pt.y + 3, 1.5f);
    }
}

void GrooveTapDelayAudioProcessorEditor::resized()
{
    auto place = [this] (juce::Component& c, int x, int y, int w, int h)
    {
        c.setBounds (juce::roundToInt (x * getWidth() / 1200.0), juce::roundToInt (y * getHeight() / 800.0),
                     juce::roundToInt (w * getWidth() / 1200.0), juce::roundToInt (h * getHeight() / 800.0));
    };
    place (patchNumberLabel, 50, 110, 54, 38);
    place (patchNameLabel, 106, 110, 210, 38);
    patchNumberLabel.setColour (juce::Label::textColourId, stompLook.amber);
    patchNumberLabel.setFont (juce::FontOptions (21));
    patchNameLabel.setFont (juce::FontOptions (18, juce::Font::bold));
    place (prevPatchButton, 42, 179, 61, 29); prevPatchButton.setButtonText ("PREV");
    place (nextPatchButton, 111, 179, 61, 29); nextPatchButton.setButtonText ("NEXT");
    place (browsePatchButton, 180, 179, 73, 29);
    place (savePatchButton, 261, 179, 63, 29);
    place (effectBandHeading, 398, 40, 390, 25);
    for (int b = 0; b < kNumBands; ++b)
    {
        place (*bandButtons[(size_t) b], 396 + b * 47, 74, 40, 46);
        place (*bandPowerButtons[(size_t) b], 396 + b * 47, 130, 40, 34);
    }
    place (editFunctionHeading, 808, 40, 350, 25);
    place (groupBox, 808, 105, 107, 34);
    place (syncBox, 925, 105, 107, 34);
    place (waveBox, 1042, 105, 118, 34);
    place (bandTitleLabel, 399, 171, 300, 23);
    bandTitleLabel.setFont (juce::FontOptions (11));
    auto knob = [&place] (juce::Slider& s, int col, int y) { place (s, 352 + col * 115, y, 105, 112); };
    knob (tapKnob, 0, 223); knob (speedKnob, 1, 223); knob (depthKnob, 2, 223);
    knob (panKnob, 3, 223); knob (levelKnob, 4, 223); knob (effectLevelKnob, 5, 223);
    knob (timeKnob, 0, 367); knob (lowCutKnob, 1, 367); knob (highCutKnob, 2, 367);
    knob (feedbackKnob, 3, 367); knob (directPanKnob, 4, 367); knob (directLevelKnob, 5, 367);
    place (outputKnob, 1060, 303, 106, 120);
    place (assignKnob, 99, 284, 160, 153);
    place (assignTarget, 99, 443, 160, 28);
    place (selectFoot, 105, 634, 175, 129);
    place (bandEnabledButton, 371, 634, 175, 129);
    place (tapTempoButton, 637, 634, 175, 129);
    place (tempoReadout, 796, 672, 87, 30);
    place (bypassButton, 903, 634, 175, 129);
}
