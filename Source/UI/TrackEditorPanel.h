#pragma once

#include "../PluginProcessor.h"
#include "Knob.h"

namespace nova
{

/**
    Sound editor for one track, organised like the hardware: SYNTH / SAMPLE / FILTER / AMP pages
    with 8 knobs each. Opens over the pads (double-click or "•••" on a pad), closes with X or a click
    next to the card.
*/
class TrackEditorPanel : public juce::Component
{
public:
    explicit TrackEditorPanel (TaktwerkProcessor& processor);

    void setTrack (int trackIndex);
    int getTrack() const noexcept { return track; }
    void setPage (takt::Page newPage);
    void refreshSampleInfo();

    std::function<void()> onClose;
    std::function<void()> onLoadSample;
    std::function<void()> onClearSample;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void attachKnobs();

    TaktwerkProcessor& proc;
    int track = 0;
    takt::Page page = takt::Page::synth;
    bool previewHeld = false;

    std::array<std::unique_ptr<Knob>, takt::knobsPerPage> knobs;
    std::array<juce::TextButton, takt::numPages> tabs;

    IconButton previewButton { IconButton::Icon::play, juce::String::fromUTF8 ("Vorhören") };
    IconButton closeButton { IconButton::Icon::close };
    juce::TextButton loadButton, clearButton;
    juce::Label sampleLabel;

    juce::Rectangle<int> card, header, knobArea, footer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackEditorPanel)
};

} // namespace nova
