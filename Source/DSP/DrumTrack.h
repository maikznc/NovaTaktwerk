#pragma once

#include "DrumSynth.h"
#include "SampleData.h"

namespace takt
{

/**
    One complete drum track:

        SYNTH + SAMPLE  ->  FILTER (7 types, ADSR)  ->  overdrive  ->  AMP envelope  ->  pan / volume
                                                                                     ->  delay / reverb sends
*/
class DrumTrack
{
public:
    void prepare (double newSampleRate) noexcept;
    void setTrack (const TrackInfo& info) noexcept { synth.setTrack (info); }
    void setParams (const TrackParams& newParams) noexcept { params = newParams; }
    void setSample (SampleData::Ptr sample) noexcept { player.setSample (std::move (sample)); }

    void trigger (float velocity) noexcept;
    void release() noexcept { filterEnv.noteOff(); }
    void choke() noexcept;

    /** Adds this track to the stereo output and the (mono) FX send buses. */
    void render (float* left, float* right, float* delaySend, float* reverbSend, int numSamples) noexcept;

    /** Peak level since the last call (for the pad lights). */
    float getPeakAndReset() noexcept
    {
        const auto p = peak;
        peak = 0.0f;
        return p;
    }

private:
    float applyFilter (float x, int type) noexcept;
    void stopSound() noexcept;

    DrumSynth synth;
    SamplePlayer player;
    dsp::Svf stage1, stage2;
    dsp::AdsrEnvelope filterEnv;
    dsp::AhdEnvelope ampEnv;
    dsp::Overdrive overdrive;

    TrackParams params;
    double sampleRate = 48000.0;

    float logCutoff = std::log (20000.0f);
    float smoothCoeff = 0.0f;
    int filterCountdown = 0;
    float resoQ = 0.7f, peakBoost = 1.0f;

    float gainL = 0.0f, gainR = 0.0f, sendDelay = 0.0f, sendReverb = 0.0f;

    float lastInput = 0.0f, declick = 0.0f, declickCoeff = 0.0f;
    float chokeGain = 1.0f, chokeCoeff = 1.0f;
    bool choking = false;

    int idleSamples = 0;
    float peak = 0.0f;
};

} // namespace takt
