#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace takt;

namespace
{
    constexpr double maxSampleSeconds = 30.0;
}

TaktwerkProcessor::TaktwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NovaTaktwerk", params::createLayout())
{
    formatManager.registerBasicFormats();

    for (int t = 0; t < numTracks; ++t)
    {
        tracks[(size_t) t].setTrack (trackInfos()[(size_t) t]);
        tracks[(size_t) t].setParams (makeDefaultTrackParams (t));

        for (int pg = 0; pg < numPages; ++pg)
            for (int i = 0; i < knobsPerPage; ++i)
                paramPointers[(size_t) t][(size_t) pg][(size_t) i] = apvts.getRawParameterValue (params::paramID (t, (Page) pg, i));
    }

    for (int i = 0; i < mst::count; ++i)
        masterPointers[(size_t) i] = apvts.getRawParameterValue (params::masterParamID (i));

    masterVolume = apvts.getRawParameterValue (params::masterVolumeID);

    // General-MIDI drum map, plus a few neighbours so common drum maps "just work"
    noteToTrack.fill (-1);
    for (int t = 0; t < numTracks; ++t)
        noteToTrack[(size_t) trackInfos()[(size_t) t].midiNote] = t;

    noteToTrack[35] = 0;              // acoustic bass drum -> BD
    noteToTrack[40] = 1;              // electric snare     -> SD
    noteToTrack[44] = closedHatTrack; // pedal hat          -> CH
    noteToTrack[48] = 7;              // hi-mid tom         -> HT
    noteToTrack[50] = 7;              // high tom           -> HT
    noteToTrack[51] = 10;             // ride               -> CY
    noteToTrack[57] = 10;             // crash 2            -> CY

    startTimerHz (2);   // frees samples the audio thread no longer uses
}

TaktwerkProcessor::~TaktwerkProcessor()
{
    stopTimer();
}

//==============================================================================
void TaktwerkProcessor::prepareToPlay (double sampleRate, int)
{
    readParameters();

    for (auto& track : tracks)
        track.prepare (sampleRate);

    masterFx.prepare (sampleRate);
    masterGain = juce::Decibels::decibelsToGain (masterVolume->load());
}

bool TaktwerkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TaktwerkProcessor::readParameters()
{
    for (size_t t = 0; t < (size_t) numTracks; ++t)
    {
        TrackParams tp;
        for (size_t pg = 0; pg < (size_t) numPages; ++pg)
            for (size_t i = 0; i < (size_t) knobsPerPage; ++i)
                tp.pages[pg][i] = paramPointers[t][pg][i]->load();

        tracks[t].setParams (tp);
    }

    MasterValues mv {};
    for (size_t i = 0; i < mv.size(); ++i)
        mv[i] = masterPointers[i]->load();

    double bpm = 128.0;
    if (auto* host = getPlayHead())
        if (const auto position = host->getPosition())
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;

    masterFx.setParameters (mv, bpm);
}

void TaktwerkProcessor::hit (int track, float velocity) noexcept
{
    tracks[(size_t) track].trigger (velocity);

    // Like on classic drum machines, the closed hat cuts off the open hat.
    if (track == closedHatTrack)
        tracks[(size_t) openHatTrack].choke();

    hitCounts[(size_t) track].fetch_add (1);
}

void TaktwerkProcessor::releaseTrack (int track) noexcept
{
    tracks[(size_t) track].release();
}

void TaktwerkProcessor::renderRange (float* left, float* right, int start, int end) noexcept
{
    while (start < end)
    {
        const auto num = std::min (end - start, MasterFx::maxBlockSize);

        std::fill (delaySend.begin(), delaySend.begin() + num, 0.0f);
        std::fill (reverbSend.begin(), reverbSend.begin() + num, 0.0f);

        for (auto& track : tracks)
            track.render (left + start, right + start, delaySend.data(), reverbSend.data(), num);

        masterFx.process (left + start, right + start, delaySend.data(), reverbSend.data(), num);
        start += num;
    }
}

void TaktwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    // Pick up newly loaded / removed samples (never blocks: if the editor is busy we try next block)
    {
        const juce::SpinLock::ScopedTryLockType lock (slotLock);
        if (lock.isLocked())
        {
            for (size_t t = 0; t < (size_t) numTracks; ++t)
            {
                if (sampleSlots[t].get() != samplesInUse[t])
                {
                    samplesInUse[t] = sampleSlots[t].get();
                    tracks[t].setSample (sampleSlots[t]);
                }
            }
        }
    }

    readParameters();

    // Pad hits / releases from the editor
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        auto handle = [this] (const std::pair<int, float>& e)
        {
            if (e.second > 0.0f) hit (e.first, e.second);
            else                 releaseTrack (e.first);
        };
        for (int i = 0; i < scope.blockSize1; ++i) handle (uiEvents[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) handle (uiEvents[(size_t) (scope.startIndex2 + i)]);
    }

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    // Sample-accurate MIDI: render up to each note, then trigger / release it
    int position = 0;
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto isOn  = message.isNoteOn();
        const auto isOff = message.isNoteOff (true);
        if (! isOn && ! isOff)
            continue;

        const auto track = noteToTrack[(size_t) message.getNoteNumber()];
        if (track < 0)
            continue;

        const auto eventPos = juce::jlimit (position, numSamples, metadata.samplePosition);
        renderRange (left, right, position, eventPos);
        position = eventPos;

        if (isOn) hit (track, message.getFloatVelocity());
        else      releaseTrack (track);
    }
    renderRange (left, right, position, numSamples);

    // Master volume (smoothed) with a soft safety clip
    const auto targetGain = juce::Decibels::decibelsToGain (masterVolume->load(), -48.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        masterGain += (targetGain - masterGain) * 0.002f;
        left[i]  = std::tanh (left[i]  * masterGain);
        right[i] = std::tanh (right[i] * masterGain);
    }

    for (size_t t = 0; t < (size_t) numTracks; ++t)
    {
        const auto peak = std::min (1.0f, tracks[t].getPeakAndReset());
        activity[t].store (std::max (peak, activity[t].load() * 0.9f));
    }

    gainReduction.store (masterFx.getGainReductionDb());
}

//==============================================================================
void TaktwerkProcessor::triggerFromUI (int track, float velocity)
{
    if (! juce::isPositiveAndBelow (track, numTracks))
        return;

    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0)      uiEvents[(size_t) scope.startIndex1] = { track, velocity };
    else if (scope.blockSize2 > 0) uiEvents[(size_t) scope.startIndex2] = { track, velocity };
}

void TaktwerkProcessor::setSampleSlot (int track, SampleData::Ptr sample)
{
    if (sample != nullptr)
    {
        const juce::ScopedLock pl (poolLock);
        samplePool.add (sample);
    }

    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        sampleSlots[(size_t) track] = sample;
    }

    ++sampleVersion;
}

juce::String TaktwerkProcessor::loadSample (int track, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (track, numTracks))
        return juce::String::fromUTF8 ("Ungültige Spur");

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        if (! file.existsAsFile())
        {
            // Remember the path anyway, so the project keeps the reference (e.g. on another computer)
            const juce::ScopedLock pl (poolLock);
            sampleFiles[(size_t) track] = file;
            sampleMissing[(size_t) track] = true;
            ++sampleVersion;
            return "Datei nicht gefunden: " + file.getFileName();
        }
        return "Dieses Format kann nicht gelesen werden: " + file.getFileName();
    }

    const auto maxLength = (juce::int64) (reader->sampleRate * maxSampleSeconds);
    const auto length = (int) std::min (reader->lengthInSamples, maxLength);

    if (length < 4)
        return "Die Datei ist leer: " + file.getFileName();

    juce::AudioBuffer<float> fileAudio ((int) reader->numChannels, length);
    reader->read (&fileAudio, 0, length, 0, true, true);

    SampleData::Ptr sample = new SampleData();
    sample->name = file.getFileNameWithoutExtension();
    sample->sourceSampleRate = reader->sampleRate;
    sample->audio.setSize (1, length);
    sample->audio.clear();

    const auto channelGain = 1.0f / (float) fileAudio.getNumChannels();
    for (int ch = 0; ch < fileAudio.getNumChannels(); ++ch)
        sample->audio.addFrom (0, 0, fileAudio, ch, 0, length, channelGain);

    {
        const juce::ScopedLock pl (poolLock);
        sampleFiles[(size_t) track] = file;
        sampleMissing[(size_t) track] = false;
    }

    setSampleSlot (track, sample);
    return {};
}

void TaktwerkProcessor::clearSample (int track)
{
    if (! juce::isPositiveAndBelow (track, numTracks))
        return;

    {
        const juce::ScopedLock pl (poolLock);
        sampleFiles[(size_t) track] = juce::File();
        sampleMissing[(size_t) track] = false;
    }

    setSampleSlot (track, nullptr);
}

juce::String TaktwerkProcessor::getSampleName (int track) const
{
    const juce::ScopedLock pl (poolLock);
    const auto& f = sampleFiles[(size_t) track];
    return f == juce::File() ? juce::String() : f.getFileNameWithoutExtension();
}

bool TaktwerkProcessor::isSampleMissing (int track) const
{
    const juce::ScopedLock pl (poolLock);
    return sampleMissing[(size_t) track];
}

bool TaktwerkProcessor::hasSample (int track) const
{
    const juce::ScopedLock pl (poolLock);
    return sampleFiles[(size_t) track] != juce::File() && ! sampleMissing[(size_t) track];
}

void TaktwerkProcessor::setParameterValue (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void TaktwerkProcessor::resetTrack (int track)
{
    for (int pg = 0; pg < numPages; ++pg)
        for (int i = 0; i < knobsPerPage; ++i)
            if (auto* p = apvts.getParameter (params::paramID (track, (Page) pg, i)))
                p->setValueNotifyingHost (p->getDefaultValue());
}

//==============================================================================
int TaktwerkProcessor::getNumPrograms()
{
    return (int) factoryKits().size();
}

const juce::String TaktwerkProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (factoryKits()[(size_t) index].name) : juce::String();
}

void TaktwerkProcessor::setCurrentProgram (int index)
{
    // Some hosts call this with the current program when a project opens – don't overwrite the user's sound then.
    if (index != currentKit)
        loadKit (index);
}

void TaktwerkProcessor::loadKit (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->getParameterID() != params::masterVolumeID)
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    const auto& kit = factoryKits()[(size_t) index];
    for (const auto& v : kit.tracks)
        setParameterValue (params::paramID (v.track, v.page, v.index), v.value);
    for (const auto& v : kit.master)
        setParameterValue (params::masterParamID (v.index), v.value);

    currentKit = index;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void TaktwerkProcessor::timerCallback()
{
    // A sample is only referenced by the pool once neither the slot nor the audio thread holds it.
    const juce::ScopedLock pl (poolLock);
    for (int i = samplePool.size(); --i >= 0;)
        if (samplePool.getObjectPointerUnchecked (i)->getReferenceCount() == 1)
            samplePool.remove (i);
}

//==============================================================================
void TaktwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("kit", currentKit);

    auto* samples = xml->createNewChildElement ("Samples");
    {
        const juce::ScopedLock pl (poolLock);
        for (int t = 0; t < numTracks; ++t)
            if (sampleFiles[(size_t) t] != juce::File())
                samples->setAttribute (juce::String (trackInfos()[(size_t) t].shortName), sampleFiles[(size_t) t].getFullPathName());
    }

    copyXmlToBinary (*xml, destData);
}

void TaktwerkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    currentKit = juce::jlimit (0, getNumPrograms() - 1, xml->getIntAttribute ("kit", 0));
    xml->removeAttribute ("kit");

    juce::StringArray samplePaths;
    for (int t = 0; t < numTracks; ++t)
        samplePaths.add ({});

    if (auto* samples = xml->getChildByName ("Samples"))
    {
        for (int t = 0; t < numTracks; ++t)
            samplePaths.set (t, samples->getStringAttribute (juce::String (trackInfos()[(size_t) t].shortName)));

        xml->removeChildElement (samples, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    for (int t = 0; t < numTracks; ++t)
    {
        if (samplePaths[t].isNotEmpty() && juce::File::isAbsolutePath (samplePaths[t]))
            loadSample (t, juce::File (samplePaths[t]));
        else
            clearSample (t);
    }
}

//==============================================================================
juce::AudioProcessorEditor* TaktwerkProcessor::createEditor()
{
    return new TaktwerkEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TaktwerkProcessor();
}
