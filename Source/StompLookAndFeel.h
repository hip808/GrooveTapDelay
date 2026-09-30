#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// All artwork is drawn at runtime: sharp at any host scale, no baked-in controls.
class StompLookAndFeel : public juce::LookAndFeel_V4
{
public:
    const juce::Colour cream { 0xffe6d8ad }, amber { 0xffffbc43 };
    StompLookAndFeel()
    {
        setColour (juce::Label::textColourId, cream);
        setColour (juce::Slider::textBoxTextColourId, cream);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff071c1d));
        setColour (juce::ComboBox::textColourId, amber);
        setColour (juce::ComboBox::outlineColourId, cream.withAlpha (0.35f));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff102d30));
        setColour (juce::PopupMenu::textColourId, cream);
        setColour (juce::TextButton::textColourOffId, cream);
        setColour (juce::TextButton::textColourOnId, cream);
    }
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox (s);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return label;
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float start, float end, juce::Slider& slider) override
    {
        auto size = (float) juce::jmin (w, h) - 16.0f;
        auto r = juce::Rectangle<float> (size, size).withCentre ({ (float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f });
        auto c = r.getCentre();
        for (int i = 0; i <= 10; ++i)
        {
            auto a = start + (end - start) * (float) i / 10.0f;
            auto p1 = c.getPointOnCircumference (size * 0.53f, a);
            auto p2 = c.getPointOnCircumference (size * 0.59f, a);
            g.setColour (cream.withAlpha (slider.isEnabled() ? 0.75f : 0.25f));
            g.drawLine ({p1, p2}, 1.4f);
        }
        for (int shadow = 6; shadow > 0; --shadow)
        {
            g.setColour (juce::Colours::black.withAlpha (0.055f));
            g.fillEllipse (r.translated (1, 4).expanded ((float) shadow));
        }
        juce::Path body;
        for (int i = 0; i < 168; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / 168.0f;
            const float radius = size * (0.478f + 0.022f * std::cos (a * 14.0f));
            auto point = c.getPointOnCircumference (radius, a);
            if (i == 0) body.startNewSubPath (point); else body.lineTo (point);
        }
        body.closeSubPath();
        juce::ColourGradient shell (juce::Colour (0xff525956), r.getTopLeft(), juce::Colour (0xff030606), r.getBottomRight(), false);
        shell.addColour (0.30, juce::Colour (0xff202928));
        shell.addColour (0.55, juce::Colour (0xff070c0c));
        g.setGradientFill (shell); g.fillPath (body);
        g.setColour (juce::Colour (0xff010404)); g.strokePath (body, juce::PathStrokeType (1.5f));
        auto inner = r.reduced (size * 0.12f);
        g.setColour (juce::Colour (0xff727972)); g.drawEllipse (inner, 1.0f);
        g.setGradientFill ({juce::Colour (0xff343a36), inner.getTopLeft(), juce::Colour (0xff0a1010), inner.getBottomRight(), false});
        g.fillEllipse (inner.reduced (1));
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        juce::Path shine;
        shine.addCentredArc (c.x, c.y, size * 0.43f, size * 0.43f, 0, -1.1f, 0.6f, true);
        g.strokePath (shine, juce::PathStrokeType (1.2f));
        g.setColour (cream.withAlpha (slider.isEnabled() ? 1.0f : 0.3f));
        auto a = start + pos * (end - start);
        g.drawLine ({c.getPointOnCircumference (size * 0.17f, a), c.getPointOnCircumference (size * 0.40f, a)}, 4.0f);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (2);
        if ((bool) b.getProperties()["foot"])
        {
            auto c = r.getCentre().translated (0, -4);
            float d = juce::jmin (r.getWidth(), r.getHeight() - 38.0f);
            auto metal = juce::Rectangle<float> (d, d).withCentre (c);
            juce::Path nut; nut.addPolygon (c, 6, d * 0.56f, 0.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillEllipse (metal.expanded (5).translated (2, 5));
            juce::ColourGradient chrome (juce::Colour (0xfff4f5ea), metal.getTopLeft(), juce::Colour (0xffd7d7ca), metal.getBottomRight(), false);
            chrome.addColour (0.24, juce::Colour (0xff727973));
            chrome.addColour (0.44, juce::Colour (0xfff2f0df));
            chrome.addColour (0.56, juce::Colour (0xff3b4440));
            chrome.addColour (0.77, juce::Colour (0xffcbcbbf));
            g.setGradientFill (chrome); g.fillPath (nut);
            g.setColour (juce::Colours::black); g.strokePath (nut, juce::PathStrokeType (1.5f));
            g.setGradientFill (chrome); g.fillEllipse (metal.reduced (3));
            g.setColour (juce::Colour (0xff343e39)); g.drawEllipse (metal.reduced (4), 2);
            g.setColour (juce::Colour (0xfff6f4e1)); g.drawEllipse (metal.reduced (7), 2);
            g.setColour (juce::Colour (0xff333d38)); g.drawEllipse (metal.reduced (10), 1.5f);
            auto cap = metal.reduced (12).translated (0, down ? 2.0f : 0.0f);
            g.setGradientFill ({juce::Colour (0xffdddccd), cap.getTopLeft(), juce::Colour (0xff777f75), cap.getBottomRight(), false});
            g.fillEllipse (cap);
            g.setColour (juce::Colour (0xfff1efdb)); g.drawEllipse (cap, 1);
            for (int ring = 1; ring < 8; ++ring)
            {
                g.setColour (juce::Colour (0xff49564e).withAlpha (0.07f));
                g.drawEllipse (cap.reduced ((float) ring * 2), 0.5f);
            }
            g.setColour (b.getToggleState() || down ? juce::Colour (0xffff4235) : juce::Colour (0xff551914));
            g.fillEllipse (c.x - 5, r.getY(), 10, 10);
        }
        else if ((bool) b.getProperties()["selector"])
        {
            auto well = r.withTrimmedTop (13);
            g.setColour (juce::Colour (0xff060e0e)); g.fillRoundedRectangle (well, 3);
            g.setColour (cream.withAlpha (0.3f)); g.drawRoundedRectangle (well.reduced (1), 3, 1);
            auto centre = well.getCentre();
            auto lit = b.getToggleState() ? amber : juce::Colour (0xff49231c);
            g.setColour (lit.withAlpha (0.18f)); g.fillEllipse (centre.x - 9, centre.y - 9, 18, 18);
            g.setColour (lit); g.fillEllipse (centre.x - 5, centre.y - 5, 10, 10);
            if (b.getToggleState())
            {
                g.setColour (juce::Colour (0xfffff2c7)); g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5, 5);
            }
        }
        else
        {
            bool power = (bool) b.getProperties()["power"];
            auto fill = b.getToggleState() ? (power ? juce::Colour (0xff12621f) : juce::Colour (0xff9d6415)) : juce::Colour (0xff101d1e);
            g.setColour (juce::Colours::black); g.fillRoundedRectangle (r, 4);
            g.setGradientFill ({fill.brighter (hover ? 0.5f : 0.15f), r.getTopLeft(), fill.darker (down ? 0.7f : 0.3f), r.getBottomRight(), false});
            g.fillRoundedRectangle (r.reduced (2), 3);
            g.setColour (cream.withAlpha (b.hasKeyboardFocus (false) ? 0.9f : 0.25f));
            g.drawRoundedRectangle (r.reduced (2), 3, 1);
        }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setColour (cream.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        auto r = b.getLocalBounds().reduced (3);
        if ((bool) b.getProperties()["foot"]) r = r.removeFromBottom (23);
        if ((bool) b.getProperties()["selector"]) r = r.removeFromTop (14);
        auto text = (bool) b.getProperties()["power"] ? (b.getToggleState() ? "ON" : "OFF") : b.getButtonText();
        g.drawFittedText (text, r, juce::Justification::centred, 1);
    }
};
