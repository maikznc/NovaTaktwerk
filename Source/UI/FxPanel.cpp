#include "FxPanel.h"

namespace nova
{

using namespace juce;

Colour FxPanel::sectionColour (int section)
{
    switch (takt::masterSections()[(size_t) section].first)
    {
        case takt::mst::distDrive:     return colours::sample;
        case takt::mst::compThreshold: return colours::accent;
        case takt::mst::revPreDelay:   return colours::synth;
        default:                       return colours::ampPage;
    }
}

static int compressorSection()
{
    for (int s = 0; s < (int) takt::masterSections().size(); ++s)
        if (takt::masterSections()[(size_t) s].first == takt::mst::compThreshold)
            return s;
    return 0;
}

FxPanel::FxPanel (TaktwerkProcessor& processor)
    : proc (processor)
{
    const auto& sections = takt::masterSections();

    for (int s = 0; s < (int) sections.size(); ++s)
    {
        for (int i = sections[(size_t) s].first; i < sections[(size_t) s].first + sections[(size_t) s].count; ++i)
        {
            auto k = std::make_unique<Knob>();
            k->attach (proc.apvts, takt::params::masterParamID (i), takt::masterDefs()[(size_t) i], sectionColour (s));
            addAndMakeVisible (*k);
            knobs[(size_t) i] = std::move (k);
        }
    }

    closeButton.setTooltip (String::fromUTF8 ("Schließen"));
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
}

void FxPanel::setGainReduction (float decibels)
{
    const auto shown = jmax (decibels, gainReduction * 0.85f);
    if (std::abs (shown - gainReduction) > 0.05f)
    {
        gainReduction = shown < 0.05f ? 0.0f : shown;
        repaint (sectionAreas[(size_t) compressorSection()]);
    }
}

void FxPanel::paint (Graphics& g)
{
    paintOverlayCard (g, getLocalBounds(), card);

    auto h = header;
    g.setColour (colours::accent);
    g.setFont (font (22.0f, true));
    g.drawText ("FX & MASTER", h.removeFromLeft (190), Justification::centredLeft, false);

    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText (String::fromUTF8 ("Spuren → Delay/Reverb (Sends im AMP-Menü) → Distortion → Kompressor → Master"),
                h, Justification::centredLeft, true);

    const auto& sections = takt::masterSections();
    for (int s = 0; s < (int) sections.size(); ++s)
        paintSection (g, sectionAreas[(size_t) s], sections[(size_t) s].title, sectionColour (s));

    // gain reduction meter next to the compressor title
    auto meter = sectionAreas[(size_t) compressorSection()].reduced (12, 0).removeFromTop (26).removeFromRight (150).withSizeKeepingCentre (150, 8);
    g.setColour (colours::outline);
    g.fillRoundedRectangle (meter.toFloat(), 4.0f);
    const auto fraction = jlimit (0.0f, 1.0f, gainReduction / 20.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (meter.toFloat().withWidth ((float) meter.getWidth() * fraction), 4.0f);
    g.setColour (colours::textDim);
    g.setFont (font (11.0f));
    g.drawText ("GR " + (gainReduction < 0.05f ? String ("0.0") : String (-gainReduction, 1)) + " dB", meter.translated (-80, 0).withWidth (74).expanded (0, 4),
                Justification::centredRight, false);
}

void FxPanel::resized()
{
    const auto bounds = getLocalBounds();
    card = bounds.withSizeKeepingCentre (jmin (bounds.getWidth() - 32, 960), jmin (bounds.getHeight() - 24, 420));

    auto inner = card.reduced (18);
    header = inner.removeFromTop (36);
    closeButton.setBounds (header.withTrimmedLeft (header.getWidth() - 34).withSizeKeepingCentre (32, 32));
    header.removeFromRight (40);

    inner.removeFromTop (10);
    const auto gap = 12;
    const auto rowHeight = (inner.getHeight() - gap) / 2;
    auto row1 = inner.removeFromTop (rowHeight);
    inner.removeFromTop (gap);
    auto row2 = inner;

    const auto& sections = takt::masterSections();

    auto splitRow = [&] (Rectangle<int> row, int a, int b)
    {
        const auto total = sections[(size_t) a].count + sections[(size_t) b].count;
        const auto widthA = (row.getWidth() - gap) * sections[(size_t) a].count / total;
        sectionAreas[(size_t) a] = row.removeFromLeft (widthA);
        row.removeFromLeft (gap);
        sectionAreas[(size_t) b] = row;
    };

    splitRow (row1, 0, 1);
    splitRow (row2, 2, 3);

    for (int s = 0; s < (int) sections.size(); ++s)
    {
        auto area = sectionAreas[(size_t) s].reduced (6, 6).withTrimmedTop (22);
        const auto count = sections[(size_t) s].count;
        const auto width = jmin (96, area.getWidth() / count);
        area = area.withSizeKeepingCentre (width * count, area.getHeight());

        for (int i = sections[(size_t) s].first; i < sections[(size_t) s].first + count; ++i)
            knobs[(size_t) i]->setBounds (area.removeFromLeft (width));
    }
}

void FxPanel::mouseDown (const MouseEvent& e)
{
    if (! card.contains (e.getPosition()) && onClose)
        onClose();
}

} // namespace nova
