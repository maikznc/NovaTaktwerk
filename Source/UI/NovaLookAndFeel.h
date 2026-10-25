#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nova
{

/** NOVA colour palette. */
namespace colours
{
    inline const juce::Colour background   { 0xff121317 };
    inline const juce::Colour header       { 0xff17181d };
    inline const juce::Colour panel        { 0xff1b1d23 };
    inline const juce::Colour pad          { 0xff22242b };
    inline const juce::Colour padHover     { 0xff282a32 };
    inline const juce::Colour outline      { 0xff33363f };
    inline const juce::Colour text         { 0xffece8e1 };
    inline const juce::Colour textDim      { 0xff8b8f99 };
    inline const juce::Colour accent       { 0xffffb547 };   // NOVA amber: hits, selection, brand
    inline const juce::Colour synth        { 0xff2bc3a1 };   // synth layer
    inline const juce::Colour sample       { 0xffff6b4a };   // sample layer
    inline const juce::Colour knobBody     { 0xff2c2f37 };
    inline const juce::Colour ampPage      { 0xff9b87f5 };   // AMP page / delay

    /** Colour of each sound-editor page: SYNTH, SAMPLE, FILTER, AMP */
    inline juce::Colour forPage (int page)
    {
        switch (page)
        {
            case 0:  return synth;
            case 1:  return sample;
            case 2:  return accent;
            default: return ampPage;
        }
    }
}

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

/** Four-pointed star used as the NOVA mark. */
inline juce::Path starPath (juce::Rectangle<float> area)
{
    juce::Path p;
    const auto c = area.getCentre();
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto inner = r * 0.22f;

    for (int i = 0; i < 8; ++i)
    {
        const auto angle  = juce::MathConstants<float>::halfPi * 0.5f * (float) i - juce::MathConstants<float>::halfPi;
        const auto radius = (i % 2 == 0) ? r : inner;
        const juce::Point<float> pt { c.x + std::cos (angle) * radius, c.y + std::sin (angle) * radius };

        if (i == 0) p.startNewSubPath (pt);
        else        p.lineTo (pt);
    }
    p.closeSubPath();
    return p;
}

class NovaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NovaLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

} // namespace nova
