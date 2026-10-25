#pragma once

#include "../TrackInfo.h"
#include "NovaLookAndFeel.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova
{

/** The small "⋯" button in the corner of every pad. */
class DotsButton : public juce::Button
{
public:
    DotsButton() : juce::Button ("Optionen") {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);

        if (highlighted || down)
        {
            g.setColour (colours::text.withAlpha (down ? 0.18f : 0.1f));
            g.fillRoundedRectangle (area, 5.0f);
        }

        const auto dot = juce::jmax (2.5f, area.getHeight() * 0.13f);
        const auto gap = dot * 2.2f;
        const auto c = area.getCentre();

        g.setColour (highlighted ? colours::text : colours::textDim);
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ c.x + (float) i * gap, c.y }));
    }
};

/**
    One of the 12 drum pads.

    Click         -> plays the track (higher click = harder hit)
    Double-click  -> opens the sound editor
    "⋯" / right-click -> pad menu
    Drop an audio file on it -> loads the sample layer
*/
class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::SettableTooltipClient
{
public:
    PadComponent (int trackIndex, const std::atomic<float>* synthLevel, const std::atomic<float>* sampleLevel);

    std::function<void (float velocity)> onHit;
    std::function<void()> onRelease;
    std::function<void()> onEdit;
    std::function<void()> onLoadSample;
    std::function<void()> onClearSample;
    std::function<void()> onReset;
    std::function<void (const juce::File&)> onFileDropped;

    void flash() { flashAmount = 1.0f; }
    void setActivity (float level);
    void setSelected (bool shouldBeSelected);
    void setSampleInfo (const juce::String& name, bool missing);

    /** Called by the editor's timer (~30 Hz) to animate the light. */
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void showMenu();

    const int track;
    const takt::TrackInfo& info;
    const std::atomic<float>* synthLevel;
    const std::atomic<float>* sampleLevel;

    DotsButton dots;

    float flashAmount = 0.0f;
    float activity = 0.0f;
    float lastSynthLevel = -1.0f, lastSampleLevel = -1.0f;
    bool selected = false;
    bool pressed = false;
    bool dragOver = false;
    juce::String sampleName;
    bool sampleMissing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};

} // namespace nova
