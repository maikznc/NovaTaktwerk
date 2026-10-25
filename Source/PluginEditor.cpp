#include "PluginEditor.h"

using namespace juce;
using namespace nova;

namespace
{
    constexpr int headerHeight = 64;
    constexpr int footerHeight = 34;
}

TaktwerkEditor::TaktwerkEditor (TaktwerkProcessor& p)
    : AudioProcessorEditor (&p), proc (p), trackEditor (p), fxPanel (p)
{
    for (int t = 0; t < takt::numTracks; ++t)
    {
        auto* synthLevel  = proc.apvts.getRawParameterValue (takt::params::paramID (t, takt::Page::synth,  takt::syn::level));
        auto* sampleLevel = proc.apvts.getRawParameterValue (takt::params::paramID (t, takt::Page::sample, takt::smp::level));

        auto pad = std::make_unique<PadComponent> (t, synthLevel, sampleLevel);
        pad->onHit         = [this, t] (float velocity) { proc.triggerFromUI (t, velocity); };
        pad->onRelease     = [this, t] { proc.triggerFromUI (t, 0.0f); };
        pad->onEdit        = [this, t] { openTrackEditor (t); };
        pad->onLoadSample  = [this, t] { chooseSample (t); };
        pad->onClearSample = [this, t] { proc.clearSample (t); };
        pad->onReset       = [this, t] { proc.resetTrack (t); showStatus (String (takt::trackInfos()[(size_t) t].shortName) + String::fromUTF8 (" zurückgesetzt"), false); };
        pad->onFileDropped = [this, t] (const File& f) { loadSample (t, f); };

        addAndMakeVisible (*pad);
        pads[(size_t) t] = std::move (pad);

        lastHitCounts[(size_t) t] = proc.getHitCount (t);
    }

    // kit selector
    for (int k = 0; k < proc.getNumPrograms(); ++k)
        kitBox.addItem (proc.getProgramName (k), k + 1);
    kitBox.setSelectedId (proc.getCurrentKit() + 1, dontSendNotification);
    kitBox.setTooltip (String::fromUTF8 ("Werks-Kit laden (überschreibt die Einstellungen, Samples bleiben)"));
    kitBox.onChange = [this]
    {
        const auto kit = kitBox.getSelectedId() - 1;
        if (kit >= 0)
        {
            proc.loadKit (kit);
            showStatus ("Kit geladen: " + proc.getProgramName (kit), false);
        }
    };
    addAndMakeVisible (kitBox);

    fxButton.setButtonText ("FX / MASTER");
    fxButton.setTooltip (String::fromUTF8 ("Distortion, Kompressor, Reverb und Delay"));
    fxButton.onClick = [this] { fxPanel.isVisible() ? closeFx() : openFx(); };
    addAndMakeVisible (fxButton);

    masterKnob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    masterKnob.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    masterKnob.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    masterKnob.setPopupDisplayEnabled (true, true, this);
    masterKnob.setTooltip (String::fromUTF8 ("Master-Lautstärke · Doppelklick: zurücksetzen"));
    masterAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, takt::params::masterVolumeID, masterKnob);
    masterKnob.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (masterKnob);

    trackEditor.onClose       = [this] { closeTrackEditor(); };
    trackEditor.onLoadSample  = [this] { chooseSample (trackEditor.getTrack()); };
    trackEditor.onClearSample = [this] { proc.clearSample (trackEditor.getTrack()); };
    addChildComponent (trackEditor);

    fxPanel.onClose = [this] { closeFx(); };
    addChildComponent (fxPanel);

    lastSampleFolder = File::getSpecialLocation (File::userMusicDirectory);

    // set the look last, so every child (incl. the knob value boxes) picks it up
    setLookAndFeel (&lookAndFeel);

    setResizable (true, true);
    setResizeLimits (860, 560, 1600, 1000);
    setSize (1000, 580);

    refreshSamples();
    startTimerHz (30);
}

TaktwerkEditor::~TaktwerkEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TaktwerkEditor::paint (Graphics& g)
{
    g.fillAll (colours::background);

    // header
    g.setColour (colours::header);
    g.fillRect (headerArea);
    g.setColour (colours::outline);
    g.fillRect (headerArea.withTop (headerArea.getBottom() - 1));

    auto brand = brandArea;

    const auto star = brand.removeFromLeft (22).toFloat().withSizeKeepingCentre (20.0f, 20.0f);
    g.setColour (colours::accent);
    g.fillPath (starPath (star));
    brand.removeFromLeft (10);

    const auto novaFont = font (22.0f, true).withExtraKerningFactor (0.22f);
    g.setFont (novaFont);
    g.setColour (colours::text);
    const auto novaWidth = GlyphArrangement::getStringWidthInt (novaFont, "NOVA") + 14;
    g.drawText ("NOVA", brand.removeFromLeft (novaWidth), Justification::centredLeft, false);

    g.setColour (colours::outline.brighter (0.3f));
    g.fillRect (brand.removeFromLeft (1).withSizeKeepingCentre (1, 22));
    brand.removeFromLeft (14);

    g.setFont (font (22.0f).withExtraKerningFactor (0.22f));
    g.setColour (colours::textDim);
    g.drawText ("TAKTWERK", brand, Justification::centredLeft, false);

    // labels
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.1f));
    g.setColour (colours::textDim);
    g.drawText ("KIT", kitBox.getBounds().translated (-40, 0).withWidth (34), Justification::centredRight, false);
    g.drawText ("MASTER", masterKnob.getBounds().translated (-62, 0).withWidth (56), Justification::centredRight, false);

    // footer
    auto footer = footerArea.reduced (24, 0);
    g.setFont (font (12.5f));

    if (statusText.isNotEmpty())
    {
        g.setColour (statusIsError ? colours::sample : colours::accent);
        g.drawText (statusText, footer, Justification::centredLeft, true);
    }
    else
    {
        g.setColour (colours::textDim);
        g.drawText (String::fromUTF8 ("Klick: spielen (oben = laut)   ·   Doppelklick oder •••: Sound bearbeiten   ·   Audiodatei aufs Pad ziehen: Sample-Layer"),
                    footer, Justification::centredLeft, true);
    }
}

void TaktwerkEditor::resized()
{
    auto bounds = getLocalBounds();
    headerArea = bounds.removeFromTop (headerHeight);
    footerArea = bounds.removeFromBottom (footerHeight);

    auto h = headerArea.reduced (24, 0);
    masterKnob.setBounds (h.removeFromRight (46).withSizeKeepingCentre (46, 46));
    h.removeFromRight (70);
    fxButton.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 32));
    h.removeFromRight (16);

    brandArea = h.removeFromLeft (300);
    h.removeFromLeft (44);
    const auto kitWidth = jmin (180, h.getWidth());
    kitBox.setBounds (h.removeFromLeft (kitWidth).withSizeKeepingCentre (kitWidth, 30));

    // 2 x 6 pad grid, square pads, centred
    auto padArea = bounds.reduced (28, 20);
    const auto gap = jlimit (10, 18, padArea.getWidth() / 60);
    const auto size = jmin ((padArea.getWidth() - 5 * gap) / 6, (padArea.getHeight() - gap) / 2);
    auto grid = padArea.withSizeKeepingCentre (size * 6 + gap * 5, size * 2 + gap);

    for (int t = 0; t < takt::numTracks; ++t)
    {
        const auto row = t / 6;
        const auto col = t % 6;
        pads[(size_t) t]->setBounds (grid.getX() + col * (size + gap), grid.getY() + row * (size + gap), size, size);
    }

    trackEditor.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
    fxPanel.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
}

//==============================================================================
void TaktwerkEditor::timerCallback()
{
    for (int t = 0; t < takt::numTracks; ++t)
    {
        auto& pad = *pads[(size_t) t];
        const auto hits = proc.getHitCount (t);

        if (hits != lastHitCounts[(size_t) t])
        {
            lastHitCounts[(size_t) t] = hits;
            pad.flash();
        }

        pad.setActivity (proc.getActivity (t));
        pad.tick();
    }

    if (proc.getSampleVersion() != lastSampleVersion)
        refreshSamples();

    if (kitBox.getSelectedId() != proc.getCurrentKit() + 1)
        kitBox.setSelectedId (proc.getCurrentKit() + 1, dontSendNotification);

    if (fxPanel.isVisible())
        fxPanel.setGainReduction (proc.getGainReduction());

    if (statusTicks > 0 && --statusTicks == 0)
    {
        statusText.clear();
        repaint (footerArea);
    }
}

void TaktwerkEditor::openTrackEditor (int track, takt::Page page)
{
    closeFx();
    trackEditor.setTrack (track);
    trackEditor.setPage (page);
    trackEditor.setVisible (true);
    trackEditor.toFront (false);

    for (int t = 0; t < takt::numTracks; ++t)
        pads[(size_t) t]->setSelected (t == track);
}

void TaktwerkEditor::closeTrackEditor()
{
    trackEditor.setVisible (false);

    for (auto& pad : pads)
        pad->setSelected (false);
}

void TaktwerkEditor::openFx()
{
    closeTrackEditor();
    fxPanel.setVisible (true);
    fxPanel.toFront (false);
    fxButton.setToggleState (true, dontSendNotification);
}

void TaktwerkEditor::closeFx()
{
    fxPanel.setVisible (false);
    fxButton.setToggleState (false, dontSendNotification);
}

void TaktwerkEditor::chooseSample (int track)
{
    const auto& info = takt::trackInfos()[(size_t) track];
    fileChooser = std::make_unique<FileChooser> (String::fromUTF8 ("Sample für ") + info.shortName + " (" + info.longName + ") laden",
                                                 lastSampleFolder,
                                                 proc.getFormatManager().getWildcardForAllFormats());

    fileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [safeThis = SafePointer<TaktwerkEditor> (this), track] (const FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile())
                                      safeThis->loadSample (track, file);
                              });
}

void TaktwerkEditor::loadSample (int track, const File& file)
{
    lastSampleFolder = file.getParentDirectory();
    const auto error = proc.loadSample (track, file);

    if (error.isNotEmpty())
        showStatus (error, true);
    else
        showStatus (file.getFileName() + String::fromUTF8 (" → ") + takt::trackInfos()[(size_t) track].shortName, false);
}

void TaktwerkEditor::refreshSamples()
{
    lastSampleVersion = proc.getSampleVersion();

    for (int t = 0; t < takt::numTracks; ++t)
        pads[(size_t) t]->setSampleInfo (proc.getSampleName (t), proc.isSampleMissing (t));

    trackEditor.refreshSampleInfo();
}

void TaktwerkEditor::showStatus (const String& message, bool isError)
{
    statusText = message;
    statusIsError = isError;
    statusTicks = 30 * 4;
    repaint (footerArea);
}
