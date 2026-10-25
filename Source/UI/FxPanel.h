#pragma once

#include "../PluginProcessor.h"
#include "Knob.h"

namespace nova
{

/** FX / master window: distortion, compressor, reverb and delay. */
class FxPanel : public juce::Component
{
public:
    explicit FxPanel (TaktwerkProcessor& processor);

    std::function<void()> onClose;

    /** Called by the editor's timer with the compressor's current gain reduction. */
    void setGainReduction (float decibels);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    static juce::Colour sectionColour (int section);

    TaktwerkProcessor& proc;
    std::array<std::unique_ptr<Knob>, takt::mst::count> knobs;
    IconButton closeButton { IconButton::Icon::close };

    juce::Rectangle<int> card, header;
    std::array<juce::Rectangle<int>, 4> sectionAreas;
    float gainReduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPanel)
};

} // namespace nova
