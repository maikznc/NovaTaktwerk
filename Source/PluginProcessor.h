#pragma once

#include "DSP/DrumTrack.h"
#include "DSP/MasterFx.h"
#include "Kits.h"
#include "Parameters.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

class TaktwerkProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    TaktwerkProcessor();
    ~TaktwerkProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    // Factory kits are exposed to the host as programs
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentKit; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Called from the editor (message thread)

    /** Plays a track as if its pad had been hit (velocity > 0), or releases it (velocity 0). */
    void triggerFromUI (int track, float velocity);

    /** Loads an audio file as the track's sample layer. Returns an error text, or empty on success. */
    juce::String loadSample (int track, const juce::File& file);
    void clearSample (int track);

    juce::String getSampleName (int track) const;
    bool isSampleMissing (int track) const;
    bool hasSample (int track) const;

    /** Sets all of a track's parameters back to their defaults. */
    void resetTrack (int track);

    /** Loads a factory kit (always, even if it is already the current one). */
    void loadKit (int index);
    int getCurrentKit() const noexcept { return currentKit; }

    juce::uint32 getHitCount (int track) const noexcept { return hitCounts[(size_t) track].load(); }
    float getActivity (int track) const noexcept { return activity[(size_t) track].load(); }
    float getGainReduction() const noexcept { return gainReduction.load(); }
    int getSampleVersion() const noexcept { return sampleVersion.load(); }

    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;
    void readParameters();
    void hit (int track, float velocity) noexcept;
    void releaseTrack (int track) noexcept;
    void renderRange (float* left, float* right, int start, int end) noexcept;
    void setSampleSlot (int track, takt::SampleData::Ptr sample);
    void setParameterValue (const juce::String& id, float value);

    std::array<takt::DrumTrack, takt::numTracks> tracks;
    takt::MasterFx masterFx;

    using PagePointers = std::array<std::atomic<float>*, takt::knobsPerPage>;
    std::array<std::array<PagePointers, takt::numPages>, takt::numTracks> paramPointers {};
    std::array<std::atomic<float>*, takt::mst::count> masterPointers {};
    std::atomic<float>* masterVolume = nullptr;
    float masterGain = 0.0f;

    std::array<float, takt::MasterFx::maxBlockSize> delaySend {}, reverbSend {};
    std::array<int, 128> noteToTrack {};

    int currentKit = 0;

    // pad hits coming from the editor
    juce::AbstractFifo uiFifo { 128 };
    std::array<std::pair<int, float>, 128> uiEvents {};

    // sample layer handling: the message thread fills the slots, the audio thread picks them up
    juce::AudioFormatManager formatManager;
    juce::SpinLock slotLock;
    std::array<takt::SampleData::Ptr, takt::numTracks> sampleSlots;
    std::array<takt::SampleData*, takt::numTracks> samplesInUse {};
    juce::CriticalSection poolLock;
    juce::ReferenceCountedArray<takt::SampleData> samplePool;   // keeps samples alive until the audio thread let go
    std::array<juce::File, takt::numTracks> sampleFiles;
    std::array<bool, takt::numTracks> sampleMissing {};
    std::atomic<int> sampleVersion { 0 };

    std::array<std::atomic<juce::uint32>, takt::numTracks> hitCounts {};
    std::array<std::atomic<float>, takt::numTracks> activity {};
    std::atomic<float> gainReduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaktwerkProcessor)
};
