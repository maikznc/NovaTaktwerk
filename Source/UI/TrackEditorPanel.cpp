#include "TrackEditorPanel.h"

namespace nova
{

using namespace juce;
using takt::Page;

TrackEditorPanel::TrackEditorPanel (TaktwerkProcessor& processor)
    : proc (processor)
{
    for (auto& k : knobs)
    {
        k = std::make_unique<Knob>();
        addAndMakeVisible (*k);
    }

    for (int i = 0; i < takt::numPages; ++i)
    {
        auto& tab = tabs[(size_t) i];
        tab.setButtonText (takt::pageNames[(size_t) i]);
        tab.setRadioGroupId (4711);
        tab.setClickingTogglesState (false);
        tab.setColour (TextButton::buttonOnColourId, colours::forPage (i));
        tab.setColour (TextButton::textColourOnId, colours::background);
        tab.onClick = [this, i] { setPage ((Page) i); };
        addAndMakeVisible (tab);
    }

    previewButton.setTriggeredOnMouseDown (true);
    previewButton.onClick = [this]
    {
        previewHeld = true;
        proc.triggerFromUI (track, 0.85f);
    };
    previewButton.onStateChange = [this]
    {
        if (previewHeld && ! previewButton.isDown())
        {
            previewHeld = false;
            proc.triggerFromUI (track, 0.0f);   // note-off → filter envelope release
        }
    };
    addAndMakeVisible (previewButton);

    closeButton.setTooltip (String::fromUTF8 ("Schließen"));
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    loadButton.setButtonText (String::fromUTF8 ("Laden …"));
    loadButton.onClick = [this] { if (onLoadSample) onLoadSample(); };
    addAndMakeVisible (loadButton);

    clearButton.setButtonText ("Entfernen");
    clearButton.onClick = [this] { if (onClearSample) onClearSample(); };
    addAndMakeVisible (clearButton);

    sampleLabel.setFont (font (12.5f));
    sampleLabel.setJustificationType (Justification::centredLeft);
    sampleLabel.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (sampleLabel);

    setTrack (0);
}

void TrackEditorPanel::setTrack (int trackIndex)
{
    track = trackIndex;
    attachKnobs();
    refreshSampleInfo();
    repaint();
}

void TrackEditorPanel::setPage (Page newPage)
{
    page = newPage;
    attachKnobs();
    repaint();
}

void TrackEditorPanel::attachKnobs()
{
    const auto& defs = takt::pageDefs (track, page);
    const auto colour = colours::forPage ((int) page);

    for (int i = 0; i < takt::knobsPerPage; ++i)
        knobs[(size_t) i]->attach (proc.apvts, takt::params::paramID (track, page, i), defs[(size_t) i], colour);

    for (int i = 0; i < takt::numPages; ++i)
        tabs[(size_t) i].setToggleState (i == (int) page, dontSendNotification);
}

void TrackEditorPanel::refreshSampleInfo()
{
    const auto name = proc.getSampleName (track);
    const auto missing = proc.isSampleMissing (track);

    if (name.isEmpty())
    {
        sampleLabel.setText (String::fromUTF8 ("Sample-Layer: leer – Audiodatei aufs Pad ziehen oder laden"), dontSendNotification);
        sampleLabel.setColour (Label::textColourId, colours::textDim);
    }
    else if (missing)
    {
        sampleLabel.setText ("Sample-Layer: Datei fehlt (" + name + ")", dontSendNotification);
        sampleLabel.setColour (Label::textColourId, colours::textDim);
    }
    else
    {
        sampleLabel.setText ("Sample-Layer: " + name, dontSendNotification);
        sampleLabel.setColour (Label::textColourId, colours::sample);
    }

    clearButton.setEnabled (name.isNotEmpty());
}

void TrackEditorPanel::paint (Graphics& g)
{
    paintOverlayCard (g, getLocalBounds(), card);

    // header: track name
    const auto& info = takt::trackInfos()[(size_t) track];
    auto h = header;

    g.setColour (colours::accent);
    g.setFont (font (26.0f, true));
    const auto nameWidth = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), info.shortName) + 12;
    g.drawText (info.shortName, h.removeFromLeft (nameWidth), Justification::centredLeft, false);

    g.setColour (colours::text);
    g.setFont (font (17.0f));
    g.drawText (info.longName, h.removeFromLeft (120), Justification::centredLeft, false);

    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("MIDI " + MidiMessage::getMidiNoteName (info.midiNote, true, true, 3) + " (" + String (info.midiNote) + ")",
                h.removeFromLeft (110), Justification::centredLeft, false);

    // knob page background with page colour line
    const auto area = knobArea.toFloat();
    g.setColour (colours::pad.withAlpha (0.55f));
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (colours::forPage ((int) page));
    g.fillRoundedRectangle (area.withHeight (3.0f).reduced (14.0f, 0.0f), 1.5f);

    // footer separator
    g.setColour (colours::outline);
    g.fillRect (footer.withHeight (1).translated (0, -6));
}

void TrackEditorPanel::resized()
{
    const auto bounds = getLocalBounds();
    card = bounds.withSizeKeepingCentre (jmin (bounds.getWidth() - 32, 920), jmin (bounds.getHeight() - 24, 340));

    auto inner = card.reduced (18);
    header = inner.removeFromTop (40);

    {
        auto h = header;
        closeButton.setBounds (h.removeFromRight (34).withSizeKeepingCentre (32, 32));
        h.removeFromRight (10);
        previewButton.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 30));
        h.removeFromRight (16);

        // page tabs in the header, right-aligned before the preview button
        const auto tabWidth = jmin (86, (h.getWidth() - 260) / takt::numPages);
        auto tabRow = h.removeFromRight (tabWidth * takt::numPages).withSizeKeepingCentre (tabWidth * takt::numPages, 30);
        for (auto& tab : tabs)
            tab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (2, 0));
    }

    inner.removeFromTop (12);
    footer = inner.removeFromBottom (32);
    inner.removeFromBottom (14);
    knobArea = inner;

    {
        auto f = footer;
        clearButton.setBounds (f.removeFromRight (100));
        f.removeFromRight (6);
        loadButton.setBounds (f.removeFromRight (100));
        f.removeFromRight (12);
        sampleLabel.setBounds (f);
    }

    auto row = knobArea.reduced (10, 12).withTrimmedTop (4);
    const auto width = row.getWidth() / takt::knobsPerPage;
    row = row.withSizeKeepingCentre (width * takt::knobsPerPage, row.getHeight());
    for (auto& k : knobs)
        k->setBounds (row.removeFromLeft (width).reduced (2, 0));
}

void TrackEditorPanel::mouseDown (const MouseEvent& e)
{
    if (! card.contains (e.getPosition()) && onClose)
        onClose();
}

} // namespace nova
