#pragma once

#include "PluginProcessor.h"
#include "UI/FxPanel.h"
#include "UI/NovaLookAndFeel.h"
#include "UI/PadComponent.h"
#include "UI/TrackEditorPanel.h"

class TaktwerkEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit TaktwerkEditor (TaktwerkProcessor&);
    ~TaktwerkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void openTrackEditor (int track, takt::Page page = takt::Page::synth);
    void closeTrackEditor();
    void openFx();
    void closeFx();

private:
    void timerCallback() override;

    void chooseSample (int track);
    void loadSample (int track, const juce::File& file);
    void refreshSamples();
    void showStatus (const juce::String& message, bool isError);

    TaktwerkProcessor& proc;
    nova::NovaLookAndFeel lookAndFeel;

    std::array<std::unique_ptr<nova::PadComponent>, takt::numTracks> pads;
    nova::TrackEditorPanel trackEditor;
    nova::FxPanel fxPanel;

    juce::ComboBox kitBox;
    juce::TextButton fxButton;
    juce::Slider masterKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastSampleFolder;

    std::array<juce::uint32, takt::numTracks> lastHitCounts {};
    int lastSampleVersion = -1;

    juce::String statusText;
    bool statusIsError = false;
    int statusTicks = 0;

    juce::Rectangle<int> headerArea, footerArea, brandArea;

    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaktwerkEditor)
};
