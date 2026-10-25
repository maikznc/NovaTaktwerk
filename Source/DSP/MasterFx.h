#pragma once

#include "../ParamDefs.h"
#include "DspHelpers.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace takt
{

/**
    The FX / master section:

        tracks + delay  (+ reverb unless "Rev Post")  ->  distortion  ->  compressor  (+ reverb if "Rev Post")

    With "Rev Post" off, the reverb runs through the distortion and the compressor, which ducks it on every
    kick – the classic hard-techno rumble.
*/
class MasterFx
{
public:
    static constexpr int maxBlockSize = 1024;

    void prepare (double newSampleRate);
    void setParameters (const MasterValues& newValues, double bpm) noexcept;

    /** In place on left/right; the send buffers hold the tracks' FX sends. numSamples <= maxBlockSize. */
    void process (float* left, float* right, const float* delaySend, const float* reverbSend, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    void processDelay (float* left, float* right, const float* send, int numSamples) noexcept;
    void processReverb (const float* send, int numSamples) noexcept;
    void computeDucking (const float* left, const float* right, int numSamples) noexcept;
    void processDistortion (float* left, float* right, int numSamples) noexcept;
    void processCompressor (float* left, float* right, int numSamples) noexcept;

    double sampleRate = 48000.0;
    MasterValues values = makeDefaultMasterValues();
    double tempo = 128.0;

    // delay
    std::vector<float> delayL, delayR;
    int delayWrite = 0;
    float delaySamples = 1000.0f;
    dsp::Svf delayHpL, delayHpR, delayLpL, delayLpR;

    // reverb
    juce::Reverb reverb;
    std::vector<float> preDelay;
    int preWrite = 0;
    dsp::Svf reverbHp, reverbLp;
    std::array<float, maxBlockSize> reverbL {}, reverbR {}, duckGain {};
    dsp::Overdrive reverbDriveL, reverbDriveR;
    dsp::Svf reverbPostLpL, reverbPostLpR;
    float duckEnvelope = 0.0f;

    // distortion
    dsp::Overdrive distL, distR;
    dsp::OnePoleLowPass toneL, toneR;

    // compressor
    dsp::Svf sidechainL, sidechainR;
    float gainReductionDb = 0.0f;
    float compEnvelope = 0.0f;
};

} // namespace takt
