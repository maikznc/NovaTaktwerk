#pragma once

#include "../ParamDefs.h"
#include "NovaLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace nova
{

/** Rotary knob with its name above it, attached to one plugin parameter. */
struct Knob : public juce::Component
{
    Knob()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (slider);

        label.setJustificationType (juce::Justification::centred);
        label.setFont (font (12.0f, true));
        label.setColour (juce::Label::textColourId, colours::textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        slider.setBounds (b);
    }

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                 const takt::ParamDef& def, juce::Colour colour)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);

        if (auto* param = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));

        const bool bipolar = def.min < 0.0f && def.max > 0.0f && def.unit != takt::Unit::decibels;
        slider.getProperties().set ("bipolar", bipolar);

        label.setText (juce::String::fromUTF8 (def.name), juce::dontSendNotification);
        slider.setTooltip (juce::String::fromUTF8 (def.tooltip) + juce::String::fromUTF8 (" · Doppelklick: Standardwert"));
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        repaint();
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Small icon button (close cross or play triangle), drawn as vector so it looks the same everywhere. */
class IconButton : public juce::Button
{
public:
    enum class Icon { close, play };

    explicit IconButton (Icon iconType, const juce::String& captionText = {})
        : juce::Button (captionText), icon (iconType), caption (captionText) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto area = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colours::pad.brighter (down ? 0.15f : (highlighted ? 0.07f : 0.0f)));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        const auto iconSize = juce::jmin (12.0f, area.getHeight() * 0.4f);
        auto iconArea = caption.isEmpty() ? area.withSizeKeepingCentre (iconSize, iconSize)
                                          : area.reduced (12.0f, 0.0f).removeFromLeft (iconSize)
                                                .withSizeKeepingCentre (iconSize, iconSize);

        juce::Path p;
        if (icon == Icon::close)
        {
            p.startNewSubPath (iconArea.getTopLeft());  p.lineTo (iconArea.getBottomRight());
            p.startNewSubPath (iconArea.getTopRight()); p.lineTo (iconArea.getBottomLeft());
            g.setColour (highlighted ? colours::text : colours::textDim);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            p.addTriangle (iconArea.getTopLeft(), iconArea.getBottomLeft(), { iconArea.getRight(), iconArea.getCentreY() });
            g.setColour (colours::accent);
            g.fillPath (p);
        }

        if (caption.isNotEmpty())
        {
            g.setColour (colours::text);
            g.setFont (font (14.0f));
            g.drawText (caption, area.withTrimmedLeft (iconArea.getRight() - area.getX() + 4.0f),
                        juce::Justification::centred, false);
        }
    }

private:
    Icon icon;
    juce::String caption;
};

/** Paints the dimmed backdrop and the rounded card used by the overlay panels. */
inline void paintOverlayCard (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Rectangle<int> card)
{
    g.setColour (colours::background.withAlpha (0.78f));
    g.fillRect (bounds);

    g.setColour (colours::panel);
    g.fillRoundedRectangle (card.toFloat(), 14.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 14.0f, 1.0f);
}

/** Section background with a coloured title marker. */
inline void paintSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour colour)
{
    const auto r = area.toFloat();
    g.setColour (colours::pad.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 10.0f);

    auto titleArea = r.reduced (12.0f, 0.0f).removeFromTop (26.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (titleArea.removeFromLeft (3.0f).withSizeKeepingCentre (3.0f, 12.0f), 1.5f);
    titleArea.removeFromLeft (8.0f);
    g.setFont (font (11.5f, true));
    g.drawText (title, titleArea, juce::Justification::centredLeft, false);
}

} // namespace nova
