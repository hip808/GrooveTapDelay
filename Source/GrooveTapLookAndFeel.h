#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// Skeuomorphic look using the MOKU AI UD Stomp faceplate assets: a
// photographed/rendered black fluted knob and metallic footswitch button,
// rotated/tinted in code rather than hand-drawn vector shapes.
class GrooveTapLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrooveTapLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory (BinaryData::Knob_png, BinaryData::Knob_pngSize);
        buttonImage = juce::ImageCache::getFromMemory (BinaryData::Button_png, BinaryData::Button_pngSize);

        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffffb454));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0c0c0c));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0c0c0c));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffffb454));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff5a4a2a));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffffb454));

        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff141414));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e8b57));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);

        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffcfd8d4));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff2e8b57));

        setColour (juce::Label::textColourId, juce::Colour (0xffe8e4d8));

        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141c1a));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe8e4d8));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2f5450));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto centre = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        if (knobImage.isValid())
        {
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));
            auto dest = juce::Rectangle<float> (centre.x - diameter * 0.5f, centre.y - diameter * 0.5f, diameter, diameter);
            g.drawImage (knobImage, dest, juce::RectanglePlacement::stretchToFit);
            g.restoreState();

            // Indicator notch drawn on top so it stays crisp at small sizes.
            juce::Path pointer;
            auto r = diameter * 0.5f;
            pointer.addRoundedRectangle (-diameter * 0.035f, -r * 0.92f, diameter * 0.07f, r * 0.34f, diameter * 0.03f);
            g.setColour (juce::Colour (0xffffb454));
            g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        }
    }

    // Mark a button to use the round metal footswitch-image style; anything
    // unmarked (EFFECT BAND select, per-band ON/OFF) stays a flat LED chip.
    static void setFootswitchStyle (juce::Button& b) { b.getProperties().set ("footswitch", true); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (! button.getProperties().contains ("footswitch"))
        {
            LookAndFeel_V4::drawButtonBackground (g, button, backgroundColour, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
            return;
        }

        auto bounds = button.getLocalBounds().toFloat();

        if (buttonImage.isValid())
        {
            auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
            auto dest = bounds.withSizeKeepingCentre (diameter, diameter);
            g.setOpacity (shouldDrawButtonAsDown ? 0.75f : 1.0f);
            g.drawImage (buttonImage, dest, juce::RectanglePlacement::stretchToFit);
            g.setOpacity (1.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff222222));
            g.fillEllipse (bounds);
        }

        if (button.getToggleState())
        {
            g.setColour (juce::Colour (0xff2e8b57).withAlpha (0.55f));
            g.fillEllipse (bounds.reduced (bounds.getWidth() * 0.32f));
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (bounds.reduced (bounds.getWidth() * 0.32f));
        }
    }

    juce::Image knobImage, buttonImage;
};
