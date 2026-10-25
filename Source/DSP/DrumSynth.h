#pragma once

#include "../ParamDefs.h"
#include "DspHelpers.h"

namespace takt
{

/**
    The synthesised part of a drum track (the SYNTH page).

    One monophonic voice per track, like on a classic drum machine: a new hit restarts it.
    Parameters are read at the moment of the hit.
*/
class DrumSynth
{
public:
    void prepare (double newSampleRate) noexcept;
    void setTrack (const TrackInfo& info) noexcept { track = info; }

    void trigger (float velocity, const PageValues& synth) noexcept;
    float process() noexcept;

    bool isActive() const noexcept { return active; }
    void stop() noexcept { active = false; }

private:
    float processTonal() noexcept;
    float processSnare() noexcept;
    float processRim() noexcept;
    float processClap() noexcept;
    float processMetal() noexcept;
    float processCowbell() noexcept;

    float pitchFactor() noexcept;

    double sampleRate = 48000.0;
    TrackInfo track = trackInfos()[0];
    bool active = false;

    float amp = 0.0f;              // velocity
    float freq = 50.0f, freq2 = 100.0f;
    float sweepOctaves = 0.0f, pitch = 0.0f, pitchCoeff = 0.0f;
    float waveDrive = 1.0f;
    float snapLevel = 0.0f, noiseLevel = 0.0f, bodyLevel = 0.0f, balance = 0.5f, gain = 1.0f;
    int holdCounter = 0;

    dsp::SineOsc osc1, osc2, osc3;
    std::array<dsp::BlepSquare, 6> squares;
    dsp::Noise noise;
    dsp::DecayEnv ampEnv, noiseEnv, clickEnv, blipEnv, auxEnv;
    dsp::Svf filterA, filterB;

    // clap
    int clapCounter = 0, clapSpacing = 0, clapBurstsLeft = 0;
    float tailSeconds = 0.3f, burstLevel = 1.0f;
};

} // namespace takt
