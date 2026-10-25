#include "NovaLookAndFeel.h"

namespace nova
{

using namespace juce;

NovaLookAndFeel::NovaLookAndFeel()
{
    auto scheme = getDarkColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground,  colours::background);
    scheme.setUIColour (ColourScheme::widgetBackground,  colours::pad);
    scheme.setUIColour (ColourScheme::menuBackground,    colours::panel);
    scheme.setUIColour (ColourScheme::outline,           colours::outline);
    scheme.setUIColour (ColourScheme::defaultText,       colours::text);
    scheme.setUIColour (ColourScheme::defaultFill,       colours::accent);
    scheme.setUIColour (ColourScheme::highlightedText,   colours::background);
    scheme.setUIColour (ColourScheme::highlightedFill,   colours::accent);
    scheme.setUIColour (ColourScheme::menuText,          colours::text);
    setColourScheme (scheme);

    setColour (Slider::rotarySliderFillColourId,    colours::accent);
    setColour (Slider::rotarySliderOutlineColourId, colours::outline);
    setColour (Slider::thumbColourId,               colours::text);
    setColour (Slider::textBoxTextColourId,         colours::textDim);
    setColour (Slider::textBoxOutlineColourId,      Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId,   Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId,    colours::accent.withAlpha (0.4f));

    setColour (Label::textColourId,                 colours::text);
    setColour (TextButton::buttonColourId,          colours::pad);
    setColour (TextButton::buttonOnColourId,        colours::accent);
    setColour (TextButton::textColourOffId,         colours::text);
    setColour (TextButton::textColourOnId,          colours::background);
    setColour (ComboBox::backgroundColourId,        colours::pad);
    setColour (ComboBox::outlineColourId,           colours::outline);
    setColour (ComboBox::textColourId,              colours::text);
    setColour (ComboBox::arrowColourId,             colours::textDim);
    setColour (PopupMenu::backgroundColourId,       colours::panel);
    setColour (PopupMenu::textColourId,             colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId,  colours::background);
    setColour (TooltipWindow::backgroundColourId,   colours::panel);
    setColour (TooltipWindow::textColourId,         colours::text);
    setColour (TooltipWindow::outlineColourId,      colours::outline);
}

void NovaLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float startAngle, float endAngle, Slider& slider)
{
    const auto bounds  = Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius  = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const auto lineW   = jmax (2.5f, radius * 0.13f);
    const auto arcR    = radius - lineW * 0.5f;
    const auto angle   = startAngle + sliderPos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();
    const auto fill    = slider.findColour (Slider::rotarySliderFillColourId).withMultipliedAlpha (enabled ? 1.0f : 0.4f);

    // background arc
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::outline);
    g.strokePath (track, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc (bipolar knobs like Pan and Tune start in the middle)
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    if (std::abs (angle - from) > 0.001f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (fill);
        g.strokePath (value, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // knob body
    const auto bodyR = radius * 0.62f;
    g.setColour (colours::knobBody);
    g.fillEllipse (Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre));
    g.setColour (colours::outline.brighter (0.2f));
    g.drawEllipse (Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre), 1.0f);

    // pointer
    const auto inner = centre.getPointOnCircumference (bodyR * 0.25f, angle);
    const auto outer = centre.getPointOnCircumference (bodyR * 0.85f, angle);
    g.setColour (colours::text.withMultipliedAlpha (enabled ? 1.0f : 0.5f));
    g.drawLine ({ inner, outer }, jmax (1.8f, radius * 0.07f));
}

void NovaLookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour& backgroundColour,
                                            bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;

    if (isButtonDown)           colour = colour.brighter (0.15f);
    else if (isMouseOverButton) colour = colour.brighter (0.07f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

Font NovaLookAndFeel::getTextButtonFont (TextButton&, int buttonHeight) { return font (jmin (14.0f, (float) buttonHeight * 0.5f)); }
Font NovaLookAndFeel::getLabelFont (Label& label)                        { return label.getFont(); }
Font NovaLookAndFeel::getComboBoxFont (ComboBox&)                        { return font (13.0f); }
Font NovaLookAndFeel::getPopupMenuFont()                                  { return font (14.0f); }

Label* NovaLookAndFeel::createSliderTextBox (Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (11.5f));
    label->setColour (Label::textColourId, colours::textDim);
    label->setColour (Label::outlineColourId, Colours::transparentBlack);
    label->setColour (Label::backgroundColourId, Colours::transparentBlack);
    return label;
}

} // namespace nova
