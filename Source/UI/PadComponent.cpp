#include "PadComponent.h"

namespace nova
{

using namespace juce;

namespace
{
    const char* audioExtensions = "wav;wave;aif;aiff;flac;ogg;mp3;m4a;caf";
}

PadComponent::PadComponent (int trackIndex, const std::atomic<float>* synthLevelParam, const std::atomic<float>* sampleLevelParam)
    : track (trackIndex),
      info (takt::trackInfos()[(size_t) trackIndex]),
      synthLevel (synthLevelParam),
      sampleLevel (sampleLevelParam)
{
    addAndMakeVisible (dots);
    dots.setTooltip (String::fromUTF8 ("Optionen (Sound bearbeiten, Sample laden …)"));
    dots.onClick = [this] { showMenu(); };

    const auto noteName = MidiMessage::getMidiNoteName (info.midiNote, true, true, 3);
    setTooltip (String (info.shortName) + String::fromUTF8 (" · ") + info.longName
                + String::fromUTF8 (" — MIDI ") + noteName + " (" + String (info.midiNote) + ")");
}

void PadComponent::setActivity (float level)
{
    const auto newActivity = jmax (level, activity * 0.85f);
    if (std::abs (newActivity - activity) > 0.005f)
    {
        activity = newActivity < 0.01f ? 0.0f : newActivity;
        repaint();
    }
}

void PadComponent::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void PadComponent::setSampleInfo (const String& name, bool missing)
{
    if (name != sampleName || missing != sampleMissing)
    {
        sampleName = name;
        sampleMissing = missing;
        repaint();
    }
}

void PadComponent::tick()
{
    bool needsRepaint = false;

    if (flashAmount > 0.0f)
    {
        flashAmount *= 0.78f;
        if (flashAmount < 0.01f)
            flashAmount = 0.0f;
        needsRepaint = true;
    }

    const auto synth  = synthLevel->load();
    const auto sample = sampleLevel->load();
    if (std::abs (synth - lastSynthLevel) > 0.0001f || std::abs (sample - lastSampleLevel) > 0.0001f)
    {
        lastSynthLevel = synth;
        lastSampleLevel = sample;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void PadComponent::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
    const auto corner = jmin (14.0f, bounds.getWidth() * 0.09f);
    const auto light  = jlimit (0.0f, 1.0f, jmax (flashAmount, activity * 0.8f));

    // body
    auto base = isMouseOver (true) ? colours::padHover : colours::pad;
    if (pressed)
        base = base.brighter (0.06f);

    g.setColour (base.interpolatedWith (colours::accent, light * 0.55f));
    g.fillRoundedRectangle (bounds, corner);

    // soft light strip along the top edge – the "LED" of the pad
    {
        auto strip = bounds.reduced (corner, 0.0f).removeFromTop (3.0f).translated (0.0f, 4.0f);
        g.setColour (colours::outline.interpolatedWith (colours::accent, jmax (light, selected ? 0.6f : 0.0f)));
        g.fillRoundedRectangle (strip, 1.5f);
    }

    // border
    g.setColour (selected ? colours::accent
                          : colours::outline.interpolatedWith (colours::accent, light * 0.6f));
    g.drawRoundedRectangle (bounds, corner, selected ? 2.0f : 1.0f);

    // text
    auto content = bounds.reduced (bounds.getWidth() * 0.09f, bounds.getHeight() * 0.1f);
    const auto nameHeight = jlimit (16.0f, 30.0f, bounds.getWidth() * 0.2f);

    g.setColour (colours::text);
    g.setFont (font (nameHeight, true));
    g.drawText (info.shortName, content.removeFromTop (nameHeight + 2.0f), Justification::topLeft, false);

    g.setColour (colours::textDim.interpolatedWith (colours::text, light));
    g.setFont (font (jlimit (11.0f, 13.0f, bounds.getWidth() * 0.1f)));
    g.drawText (info.longName, content.removeFromTop (18.0f), Justification::topLeft, true);

    // level bars at the bottom: synth (top) and sample (bottom)
    const auto hasSample = sampleName.isNotEmpty() && ! sampleMissing;
    auto bars = content.removeFromBottom (11.0f);

    auto drawBar = [&g] (Rectangle<float> r, float level, Colour colour, float alpha)
    {
        g.setColour (colours::outline);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        if (level > 0.001f)
        {
            g.setColour (colour.withAlpha (alpha));
            g.fillRoundedRectangle (r.withWidth (jmax (r.getHeight(), r.getWidth() * level)), r.getHeight() * 0.5f);
        }
    };

    drawBar (bars.removeFromTop (4.0f), synthLevel->load(), colours::synth, 1.0f);
    bars.removeFromTop (3.0f);
    drawBar (bars.removeFromTop (4.0f), sampleLevel->load(), colours::sample, hasSample ? 1.0f : 0.25f);

    // sample name
    if (sampleName.isNotEmpty())
    {
        content.removeFromBottom (4.0f);
        g.setFont (font (11.0f));
        g.setColour (sampleMissing ? colours::textDim : colours::sample);
        g.drawText (sampleMissing ? "fehlt: " + sampleName : sampleName,
                    content.removeFromBottom (14.0f), Justification::bottomLeft, true);
    }

    // file drag highlight
    if (dragOver)
    {
        g.setColour (colours::background.withAlpha (0.75f));
        g.fillRoundedRectangle (bounds, corner);

        Path outline;
        outline.addRoundedRectangle (bounds.reduced (3.0f), corner);
        Path dashed;
        const float dashes[] = { 6.0f, 4.0f };
        PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (colours::sample);
        g.fillPath (dashed);

        g.setFont (font (13.0f, true));
        g.drawText ("Sample ablegen", bounds, Justification::centred, false);
    }
}

void PadComponent::resized()
{
    const auto size = jlimit (22, 30, getWidth() / 5);
    dots.setBounds (getWidth() - size - 6, 6, size, size);
}

void PadComponent::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }

    pressed = true;

    // Velocity from the click height: top of the pad = full velocity
    const auto relY = jlimit (0.0f, 1.0f, (float) e.y / (float) jmax (1, getHeight()));
    if (onHit)
        onHit (jmap (relY, 1.0f, 0.35f));

    repaint();
}

void PadComponent::mouseUp (const MouseEvent&)
{
    if (pressed && onRelease)
        onRelease();

    pressed = false;
    repaint();
}

void PadComponent::mouseDoubleClick (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && onEdit)
        onEdit();
}

bool PadComponent::isInterestedInFileDrag (const StringArray& files)
{
    for (const auto& f : files)
        if (File (f).hasFileExtension (audioExtensions))
            return true;

    return false;
}

void PadComponent::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;
    repaint();

    for (const auto& f : files)
    {
        const File file (f);
        if (file.hasFileExtension (audioExtensions))
        {
            if (onFileDropped)
                onFileDropped (file);
            return;
        }
    }
}

void PadComponent::showMenu()
{
    PopupMenu menu;
    menu.addSectionHeader (String (info.shortName) + "  " + info.longName);
    menu.addItem (1, String::fromUTF8 ("Sound bearbeiten …"));
    menu.addItem (2, String::fromUTF8 ("Vorhören"));
    menu.addSeparator();
    menu.addItem (3, String::fromUTF8 ("Sample laden …"));
    menu.addItem (4, "Sample entfernen", sampleName.isNotEmpty());
    menu.addSeparator();
    menu.addItem (5, String::fromUTF8 ("Spur zurücksetzen"));

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&dots).withMinimumWidth (190),
                        [safeThis = SafePointer<PadComponent> (this)] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            auto& pad = *safeThis;
                            switch (result)
                            {
                                case 1: if (pad.onEdit)        pad.onEdit();        break;
                                case 2: if (pad.onHit)         { pad.onHit (0.85f); } break;
                                case 3: if (pad.onLoadSample)  pad.onLoadSample();  break;
                                case 4: if (pad.onClearSample) pad.onClearSample(); break;
                                case 5: if (pad.onReset)       pad.onReset();       break;
                                default: break;
                            }
                        });
}

} // namespace nova
