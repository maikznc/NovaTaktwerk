// Checks the plug-in processor (MIDI, pad hits, samples, kits, saving/loading), renders a groove for
// every factory kit through the complete plug-in, and draws the plug-in window into PNG files.
// Usage: NovaSnapshot <output folder>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

#include <iostream>

using namespace juce;

namespace
{
int failures = 0;

void check (bool ok, const String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FAIL] ") << what << "\n";
    if (! ok) ++failures;
}

void savePng (Component& c, const File& file)
{
    const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    file.deleteFile();
    FileOutputStream out (file);
    PNGImageFormat().writeImageToStream (image, out);
    std::cout << "  Bild: " << file.getFileName() << "\n";
}

uint32 totalHits (const TaktwerkProcessor& p)
{
    uint32 n = 0;
    for (int t = 0; t < takt::numTracks; ++t) n += p.getHitCount (t);
    return n;
}

float paramValue (TaktwerkProcessor& p, const String& id)
{
    return p.apvts.getRawParameterValue (id)->load();
}

String pid (int track, takt::Page page, int index) { return takt::params::paramID (track, page, index); }

struct FixedPlayHead : public AudioPlayHead
{
    double bpm = 145.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (true);
        return info;
    }
};

void writeWav (const AudioBuffer<float>& b, const File& file)
{
    file.deleteFile();
    std::unique_ptr<OutputStream> stream = std::make_unique<FileOutputStream> (file);
    auto writer = WavAudioFormat().createWriterFor (stream, AudioFormatWriterOptions{}.withSampleRate (48000.0)
                                                                                       .withNumChannels (2)
                                                                                       .withBitsPerSample (24));
    writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

/** 4 bars of hard techno at 145 BPM through the complete plug-in. */
AudioBuffer<float> renderGroove (TaktwerkProcessor& proc, bool kickAndHatsOnly)
{
    constexpr double fs = 48000.0;
    constexpr int block = 512;
    const auto sixteenth = 60.0 / 145.0 / 4.0 * fs;
    const int steps = 64;
    const auto total = (int) (sixteenth * steps + fs * 2.0);

    struct Note { int step; int note; int vel; };
    std::vector<Note> notes;
    for (int s = 0; s < steps; ++s)
    {
        const auto inBar = s % 16;
        if (inBar % 4 == 0) notes.push_back ({ s, 36, 127 });                    // four on the floor
        if (inBar % 4 == 2) notes.push_back ({ s, 46, 90 });                     // open hat offbeat
        if (inBar % 2 == 1) notes.push_back ({ s, 42, inBar % 4 == 1 ? 70 : 55 }); // closed hat
        if (kickAndHatsOnly) continue;
        if (inBar == 4 || inBar == 12) notes.push_back ({ s, 39, 110 });          // clap
        if (inBar == 7 || inBar == 14) notes.push_back ({ s, 37, 80 });           // rim
        if (s == 0 || s == 32) notes.push_back ({ s, 49, 90 });                   // cymbal
        if (s >= 60) notes.push_back ({ s, 47 - (s - 60) * 2, 110 });             // tom fill
        if (inBar == 10 && s > 16) notes.push_back ({ s, 56, 70 });              // cowbell
        if (s == 28 || s == 61) notes.push_back ({ s, 38, 120 });                 // snare
    }

    AudioBuffer<float> out (2, total);
    out.clear();
    AudioBuffer<float> buffer (2, block);
    MidiBuffer midi;

    for (int pos = 0; pos < total; pos += block)
    {
        const auto n = std::min (block, total - pos);
        buffer.setSize (2, n, false, false, true);
        midi.clear();

        for (const auto& note : notes)
        {
            const auto on  = (int) (note.step * sixteenth);
            const auto off = (int) ((note.step + 0.5) * sixteenth);
            if (on >= pos && on < pos + n)   midi.addEvent (MidiMessage::noteOn (10, note.note, (uint8) note.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (MidiMessage::noteOff (10, note.note), off - pos);
        }

        proc.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, buffer, ch, 0, n);
    }
    return out;
}

File writeTestSample (const File& folder)
{
    const auto file = folder.getChildFile ("Testsample Shaker.wav");
    AudioBuffer<float> b (2, 22050);
    Random rng (1);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const auto v = std::exp (-(float) i / 3000.0f) * (rng.nextFloat() * 2.0f - 1.0f) * 0.8f;
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }

    file.deleteFile();
    std::unique_ptr<OutputStream> stream = std::make_unique<FileOutputStream> (file);
    auto writer = WavAudioFormat().createWriterFor (stream, AudioFormatWriterOptions{}.withSampleRate (44100.0)
                                                                                        .withNumChannels (2)
                                                                                        .withBitsPerSample (16));
    writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return file;
}
} // namespace

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI gui;
    using takt::Page;

    const auto outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshots");
    outDir.createDirectory();

    std::cout << "\nProzessor-Checks\n";

    TaktwerkProcessor proc;
    FixedPlayHead playHead;
    proc.setPlayHead (&playHead);
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    check (proc.getParameters().size() == takt::numTracks * 32 + takt::mst::count + 1,
           "Parameter: 12 Spuren x 4 Seiten x 8 + FX/Master (" + String (proc.getParameters().size()) + ")");

    AudioBuffer<float> buffer (2, 512);
    MidiBuffer midi;

    midi.addEvent (MidiMessage::noteOn (10, 36, (uint8) 110), 10);
    midi.addEvent (MidiMessage::noteOn (10, 60, (uint8) 100), 20);
    proc.processBlock (buffer, midi);
    check (proc.getHitCount (0) == 1 && totalHits (proc) == 1, "MIDI 36 spielt BD, Note 60 wird ignoriert");
    check (buffer.getMagnitude (0, 512) > 0.01f, "Audio kommt raus");

    midi.clear();
    midi.addEvent (MidiMessage::noteOn (10, 42, (uint8) 100), 0);
    midi.addEvent (MidiMessage::noteOn (10, 46, (uint8) 100), 100);
    midi.addEvent (MidiMessage::noteOff (10, 46), 300);
    midi.addEvent (MidiMessage::noteOn (10, 56, (uint8) 100), 200);
    proc.processBlock (buffer, midi);
    check (proc.getHitCount (8) == 1 && proc.getHitCount (9) == 1 && proc.getHitCount (11) == 1, "GM-Noten 42/46/56 -> CH/OH/CB (mit Note-Off)");

    midi.clear();
    proc.triggerFromUI (1, 0.9f);
    proc.triggerFromUI (1, 0.0f);
    proc.processBlock (buffer, midi);
    check (proc.getHitCount (1) == 1, "Pad-Klick aus der Oberflaeche spielt SD");

    // sample layer
    const auto sampleFile = writeTestSample (outDir);
    check (proc.loadSample (2, sampleFile).isEmpty() && proc.hasSample (2), "Sample laden auf RS");

    // kits
    proc.loadKit (1);
    check (proc.getCurrentKit() == 1 && std::abs (paramValue (proc, pid (0, Page::amp, takt::amp::reverb)) - 0.7f) < 0.001f,
           "Kit \"Rumble\": BD Reverb-Send 70 %");
    check (std::abs (paramValue (proc, takt::params::masterParamID (takt::mst::revLpf)) - 300.0f) < 1.0f, "Kit \"Rumble\": Reverb LPF 300 Hz");
    proc.loadKit (0);
    check (std::abs (paramValue (proc, pid (0, Page::amp, takt::amp::reverb))) < 0.001f, "Zurueck auf \"Hard Techno\" setzt alles zurueck");
    check (proc.hasSample (2), "Kit-Wechsel behaelt die Samples");

    proc.setCurrentProgram (0);
    check (proc.getCurrentKit() == 0, "Host-Programmwechsel auf aktuelles Kit ueberschreibt nichts");

    // saving / loading
    if (auto* hold = proc.apvts.getParameter (pid (0, Page::synth, takt::tonal::hold)))
        hold->setValueNotifyingHost (hold->convertTo0to1 (0.2f));
    proc.loadKit (2);
    if (auto* hold = proc.apvts.getParameter (pid (0, Page::synth, takt::tonal::hold)))
        hold->setValueNotifyingHost (hold->convertTo0to1 (0.2f));

    MemoryBlock state;
    proc.getStateInformation (state);
    TaktwerkProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (paramValue (restored, pid (0, Page::synth, takt::tonal::hold)) - 0.2f) < 0.002f, "Projekt speichern/laden: BD Hold bleibt 200 ms");
    check (restored.getCurrentKit() == 2, "Projekt speichern/laden: Kit bleibt \"Industrial\"");
    check (restored.getSampleName (2) == "Testsample Shaker", "Projekt speichern/laden: Sample bleibt geladen");

    //==========================================================================
    std::cout << "\nGroove pro Kit (145 BPM, 4 Takte, kompletter Plugin-Signalweg)\n";
    for (int k = 0; k < proc.getNumPrograms(); ++k)
    {
        TaktwerkProcessor p;
        p.setPlayHead (&playHead);
        p.setPlayConfigDetails (0, 2, 48000.0, 512);
        p.loadKit (k);
        p.prepareToPlay (48000.0, 512);

        const auto groove = renderGroove (p, k == 1);
        const auto peak = groove.getMagnitude (0, groove.getNumSamples());
        const auto rms  = groove.getRMSLevel (0, 0, groove.getNumSamples());
        bool finite = true;
        for (int i = 0; i < groove.getNumSamples(); ++i)
            finite = finite && std::isfinite (groove.getSample (0, i));

        const auto name = p.getProgramName (k);
        const auto file = outDir.getChildFile ("groove-" + String (k + 1) + "-" + name.replaceCharacter (' ', '-') + ".wav");
        writeWav (groove, file);
        std::cout << "  " << name.paddedRight (' ', 12) << " Peak " << String (peak, 2) << "  RMS " << String (Decibels::gainToDecibels (rms), 1)
                  << " dBFS  -> " << file.getFileName() << "\n";
        check (finite && peak <= 1.0f && peak > 0.2f, name + ": Pegel ok");
    }

    //==========================================================================
    std::cout << "\nOberflaeche\n";
    {
        std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
        auto* ed = dynamic_cast<TaktwerkEditor*> (editor.get());
        check (ed != nullptr, "Editor erstellt");
        editor->setSize (1000, 580);

        int index = 0;
        for (auto* child : editor->getChildren())
            if (auto* pad = dynamic_cast<nova::PadComponent*> (child))
            {
                if (index == 0 || index == 8) pad->flash();
                ++index;
            }

        savePng (*editor, outDir.getChildFile ("1-pads.png"));

        ed->openTrackEditor (0, Page::synth);  savePng (*editor, outDir.getChildFile ("2-bd-synth.png"));
        ed->openTrackEditor (0, Page::filter); savePng (*editor, outDir.getChildFile ("3-bd-filter.png"));
        ed->openTrackEditor (0, Page::amp);    savePng (*editor, outDir.getChildFile ("4-bd-amp.png"));
        ed->openTrackEditor (2, Page::sample); savePng (*editor, outDir.getChildFile ("5-rs-sample.png"));
        ed->openTrackEditor (9, Page::synth);  savePng (*editor, outDir.getChildFile ("6-oh-synth.png"));
        ed->openFx();                          savePng (*editor, outDir.getChildFile ("7-fx-master.png"));

        editor->setSize (860, 560);
        savePng (*editor, outDir.getChildFile ("8-fx-klein.png"));
        ed->openTrackEditor (3, Page::synth);  savePng (*editor, outDir.getChildFile ("9-cp-klein.png"));
    }

    std::cout << "\n" << (failures == 0 ? "Alle Checks bestanden." : String (failures) + " Check(s) fehlgeschlagen.") << "\n";
    return failures == 0 ? 0 : 1;
}
