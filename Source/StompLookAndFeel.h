#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// Static material sprites; pointers, values and indicator states are always live.
class StompLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Image knobImage, footImage;
public:
    const juce::Colour cream { 0xffe6d8ad }, amber { 0xffffb52c };
    static juce::Font panelFont (float height)
    {
        return juce::Font (juce::FontOptions ("Avenir Next Condensed", height * 1.38f, juce::Font::plain));
    }
    StompLookAndFeel()
    {
        auto atlas = juce::ImageCache::getFromMemory (BinaryData::controls_png, BinaryData::controls_pngSize);
        knobImage = atlas.getClippedImage ({75, 60, 745, 745});
        footImage = atlas.getClippedImage ({1024, 115, 630, 630});
        setColour (juce::Label::textColourId, cream);
        setColour (juce::Slider::textBoxTextColourId, cream);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff050909));
        setColour (juce::ComboBox::textColourId, amber);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff364640));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff102d30));
        setColour (juce::PopupMenu::textColourId, cream);
    }
    juce::Font getSliderPopupFont (juce::Slider&) override { return panelFont (18); }
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return l;
    }
    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        if (l.isBeingEdited()) return;
        const bool value = dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr;
        const bool lcd = (bool) l.getProperties()["lcd"];
        auto font = value ? panelFont ((float) l.getHeight() * 0.86f) : l.getFont();
        g.setFont (font);
        auto r = l.getLocalBounds().reduced (2, 0);
        auto text = lcd ? l.getText().toUpperCase() : l.getText();
        if (lcd)
        {
            g.setColour (amber.withAlpha (0.16f));
            for (auto offset : { juce::Point<int> (-1, 0), {1, 0}, {0, 1}, {0, -1} })
                g.drawFittedText (text, r.translated (offset.x, offset.y), l.getJustificationType(), 1);
        }
        g.setColour ((lcd ? amber : l.findColour (juce::Label::textColourId)).withMultipliedAlpha (l.isEnabled() ? 1.0f : 0.4f));
        g.drawFittedText (text, r, l.getJustificationType(), 1);
        if (lcd)
        {
            g.setColour (juce::Colours::black.withAlpha (0.23f));
            for (int y = 1; y < l.getHeight(); y += 3) g.drawHorizontalLine (y, 0, (float) l.getWidth());
        }
    }
    juce::Font getComboBoxFont (juce::ComboBox& b) override
    { return juce::Font (juce::FontOptions ("Menlo", (float) b.getHeight() * 0.44f, juce::Font::plain)); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override
    { l.setBounds (3, 0, b.getWidth()-6, b.getHeight()); l.setFont (getComboBoxFont (b)); l.setJustificationType (juce::Justification::centred); }
    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (1, 1, (float) w-2, (float) h-2);
        g.setGradientFill ({juce::Colour (0xff010202), 0, 0, juce::Colour (down ? 0xff172922 : 0xff0b1412), 0, (float) h, false});
        g.fillRoundedRectangle (r, 3);
        g.setColour (juce::Colour (0xff526059)); g.drawRoundedRectangle (r, 3, 0.8f);
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                          float pos, float start, float end, juce::Slider& s) override
    {
        const auto size = (float) juce::jmin (w, h) * 0.95f;
        auto r = juce::Rectangle<float> (size, size).withCentre ({ (float)x+(float)w*0.5f, (float)y+(float)h*0.5f });
        auto c = r.getCentre();
        g.setColour (cream.withAlpha (s.isEnabled() ? 1.0f : 0.35f));
        for (int i=0; i<=10; ++i)
        {
            auto a = start + (end-start)*(float)i/10.0f;
            g.drawLine ({ c.getPointOnCircumference (size*0.51f, a), c.getPointOnCircumference (size*0.58f, a) }, 1.4f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillEllipse (r.expanded (3).translated (2, 4));
        g.setOpacity (1); g.drawImage (knobImage, r);
        const auto a = start+pos*(end-start);
        juce::Path pointer;
        pointer.addRoundedRectangle (-size*0.025f, -size*0.40f, size*0.05f, size*0.30f, size*0.02f);
        g.setColour (cream.withAlpha (s.isEnabled() ? 1.0f : 0.35f));
        g.fillPath (pointer, juce::AffineTransform::rotation (a).translated (c.x, c.y));
    }
    void led (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, bool lit)
    {
        g.setColour (juce::Colours::black); g.fillEllipse (c.x-radius-2, c.y-radius-2, radius*2+4, radius*2+4);
        if (lit)
        {
            g.setColour (colour.withAlpha (0.18f)); g.fillEllipse (c.x-radius*1.9f, c.y-radius*1.9f, radius*3.8f, radius*3.8f);
        }
        g.setGradientFill ({lit ? colour.brighter (0.7f) : colour.darker (0.3f), c.x-radius/3, c.y-radius/3,
                            colour.darker (lit ? 0.2f : 1.2f), c.x+radius, c.y+radius, true});
        g.fillEllipse (c.x-radius, c.y-radius, radius*2, radius*2);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.9f : 0.4f));
        g.fillEllipse (c.x-radius*0.4f, c.y-radius*0.6f, radius*0.65f, radius*0.65f);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        const auto scale = r.getWidth() / ((bool)b.getProperties()["foot"] ? 155.0f : ((bool)b.getProperties()["smallSwitch"] ? 45.0f : 39.0f));
        if ((bool)b.getProperties()["foot"])
        {
            auto c = juce::Point<float> (r.getCentreX(), 98*scale);
            g.setOpacity (1); g.drawImage (footImage, juce::Rectangle<float> (71*scale, 71*scale).withCentre (c.translated (0, down ? scale : 0)));
            led (g, {c.x, 17*scale}, 8*scale, juce::Colour (0xffff2317), b.getToggleState() || down);
            return;
        }
        const bool selector = (bool)b.getProperties()["selector"];
        const bool small = (bool)b.getProperties()["smallSwitch"];
        if (selector || small) r.removeFromTop (20*scale);
        r = r.reduced (2*scale);
        if (small) r = r.withSizeKeepingCentre (32*scale, 32*scale);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r.translated (1, 2), 3);
        g.setGradientFill ({juce::Colour (0xff48534d), r.getTopLeft(), juce::Colour (0xff010202), r.getBottomRight(), false});
        g.fillRoundedRectangle (r, 3);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 3, 1.5f);
        auto inside = r.reduced (4*scale);
        const bool on = (bool)b.getProperties()["power"] && b.getToggleState();
        g.setGradientFill ({on ? juce::Colour (0xff086600) : juce::Colour (0xff081010), inside.getTopLeft(), juce::Colour (0xff010403), inside.getBottomRight(), false});
        g.fillRoundedRectangle (inside, 2);
        if (on)
        {
            g.setGradientFill ({juce::Colour (0xff24b900).withAlpha (0.7f), inside.getCentre(), juce::Colours::transparentBlack, inside.getBottomRight(), true});
            g.fillRoundedRectangle (inside, 2);
        }
        if (selector || small) led (g, r.getCentre(), 5*scale, selector && b.getToggleState() ? amber : juce::Colour (0xffff2317), b.getToggleState() || down);
        if (hover || b.hasKeyboardFocus (false)) { g.setColour (cream.withAlpha (0.45f)); g.drawRoundedRectangle (r, 3, 1); }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds();
        const bool foot = (bool)b.getProperties()["foot"];
        const bool selector = (bool)b.getProperties()["selector"];
        const bool small = (bool)b.getProperties()["smallSwitch"];
        const float scale = (float)r.getWidth() / (foot ? 155.0f : (small ? 45.0f : 39.0f));
        g.setColour (cream.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
        g.setFont (panelFont ((foot ? 19.0f : 16.0f)*scale));
        if (foot)
        {
            auto number = b.getProperties()["number"].toString();
            g.drawText (number.isEmpty() ? "BYPASS" : number, r.withY (juce::roundToInt (30*scale)).withHeight (juce::roundToInt (25*scale)), juce::Justification::centred);
            if (number.isEmpty()) return;
            r = r.removeFromBottom (juce::roundToInt (25*scale));
        }
        else if (selector || small) r = r.removeFromTop (juce::roundToInt (20*scale));
        auto text = (bool)b.getProperties()["power"] ? (b.getToggleState() ? "ON" : "OFF") : b.getButtonText();
        g.drawFittedText (text, r, juce::Justification::centred, 1);
    }
};
