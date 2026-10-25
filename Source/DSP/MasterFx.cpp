#include "MasterFx.h"

namespace takt
{

using namespace dsp;

void MasterFx::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    const auto delayLength = (size_t) (sampleRate * 4.5) + 8;   // 2 beats at 30 BPM
    delayL.assign (delayLength, 0.0f);
    delayR.assign (delayLength, 0.0f);
    delayWrite = 0;
    delaySamples = (float) (sampleRate * 0.25);

    preDelay.assign ((size_t) (sampleRate * 0.25) + 8, 0.0f);
    preWrite = 0;

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    for (auto* f : { &delayHpL, &delayHpR, &delayLpL, &delayLpR, &reverbHp, &reverbLp, &sidechainL, &sidechainR })
        f->reset();

    distL.prepare (sampleRate);
    distR.prepare (sampleRate);
    reverbDriveL.prepare (sampleRate);
    reverbDriveR.prepare (sampleRate);
    reverbPostLpL.reset();
    reverbPostLpR.reset();
    toneL.reset();
    toneR.reset();
    compEnvelope = 0.0f;
    gainReductionDb = 0.0f;
    duckEnvelope = 0.0f;
}

void MasterFx::setParameters (const MasterValues& newValues, double bpm) noexcept
{
    values = newValues;
    tempo  = std::clamp (bpm > 0.0 ? bpm : 128.0, 30.0, 300.0);

    delayHpL.setup (values[mst::dlyHpf], 0.7f, sampleRate);
    delayHpR.setup (values[mst::dlyHpf], 0.7f, sampleRate);
    delayLpL.setup (values[mst::dlyLpf], 0.7f, sampleRate);
    delayLpR.setup (values[mst::dlyLpf], 0.7f, sampleRate);

    reverbHp.setup (values[mst::revHpf], 0.7f, sampleRate);
    reverbLp.setup (values[mst::revLpf], 0.7f, sampleRate);
    reverbPostLpL.setup (values[mst::revLpf] * 1.5f, 0.7f, sampleRate);
    reverbPostLpR.setup (values[mst::revLpf] * 1.5f, 0.7f, sampleRate);

    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.2f + std::clamp (values[mst::revDecay], 0.0f, 1.0f) * 0.78f;
    rp.damping    = std::clamp (values[mst::revDamp], 0.0f, 1.0f);
    rp.wetLevel   = 0.5f;
    rp.dryLevel   = 0.0f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    const auto toneHz = 1500.0f * std::exp2 (std::clamp (values[mst::distTone], 0.0f, 1.0f) * 3.7f);
    toneL.setCutoff (toneHz, sampleRate);
    toneR.setCutoff (toneHz, sampleRate);

    const auto scHz = values[mst::compScHpf];
    sidechainL.setup (scHz, 0.7f, sampleRate);
    sidechainR.setup (scHz, 0.7f, sampleRate);
}

void MasterFx::process (float* left, float* right, const float* delaySend, const float* reverbSend, int numSamples) noexcept
{
    jassert (numSamples <= maxBlockSize);
    numSamples = std::min (numSamples, maxBlockSize);

    computeDucking (left, right, numSamples);   // from the dry tracks, before any FX is added
    processDelay (left, right, delaySend, numSamples);
    processReverb (reverbSend, numSamples);

    const bool reverbPost = values[mst::distRevPost] >= 0.5f;
    const auto reverbVolume = values[mst::revVolume];

    if (! reverbPost)
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  += reverbL[(size_t) i] * reverbVolume;
            right[i] += reverbR[(size_t) i] * reverbVolume;
        }

    processDistortion (left, right, numSamples);
    processCompressor (left, right, numSamples);

    if (reverbPost)
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  += reverbL[(size_t) i] * reverbVolume;
            right[i] += reverbR[(size_t) i] * reverbVolume;
        }
}

//==============================================================================
void MasterFx::processDelay (float* left, float* right, const float* send, int numSamples) noexcept
{
    const auto volume   = values[mst::dlyVolume];
    const auto feedback = std::clamp (values[mst::dlyFeedback], 0.0f, 1.0f) * 0.95f;
    const bool pingPong = values[mst::dlyPingPong] >= 0.5f;

    const auto division = std::clamp ((int) std::lround (values[mst::dlyTime]), 0, (int) delayBeats.size() - 1);
    const auto length   = (int) delayL.size();
    const auto target   = std::clamp ((float) (delayBeats[(size_t) division] * 60.0 / tempo * sampleRate),
                                      16.0f, (float) (length - 4));

    for (int i = 0; i < numSamples; ++i)
    {
        delaySamples += (target - delaySamples) * 0.0005f;   // glide when the tempo changes

        auto readPos = (float) delayWrite - delaySamples;
        if (readPos < 0.0f) readPos += (float) length;
        const auto i0 = (int) readPos;
        const auto i1 = (i0 + 1) % length;
        const auto fr = readPos - (float) i0;

        auto wetL = delayL[(size_t) i0] + (delayL[(size_t) i1] - delayL[(size_t) i0]) * fr;
        auto wetR = delayR[(size_t) i0] + (delayR[(size_t) i1] - delayR[(size_t) i0]) * fr;
        wetL = delayLpL.process (delayHpL.process (wetL).hp).lp;
        wetR = delayLpR.process (delayHpR.process (wetR).hp).lp;

        const auto in = send[i];
        float writeL, writeR;
        if (pingPong) { writeL = in + wetR * feedback;  writeR = wetL * feedback; }
        else          { writeL = in + wetL * feedback;  writeR = in + wetR * feedback; }

        delayL[(size_t) delayWrite] = std::tanh (writeL);
        delayR[(size_t) delayWrite] = std::tanh (writeR);
        delayWrite = (delayWrite + 1) % length;

        left[i]  += wetL * volume;
        right[i] += wetR * volume;
    }
}

void MasterFx::processReverb (const float* send, int numSamples) noexcept
{
    const auto length = (int) preDelay.size();
    const auto delay  = std::clamp ((int) (values[mst::revPreDelay] * (float) sampleRate), 0, length - 2);

    for (int i = 0; i < numSamples; ++i)
    {
        preDelay[(size_t) preWrite] = send[i];
        auto readIndex = preWrite - delay;
        if (readIndex < 0) readIndex += length;
        preWrite = (preWrite + 1) % length;

        const auto x = reverbLp.process (reverbHp.process (preDelay[(size_t) readIndex]).hp).lp;
        reverbL[(size_t) i] = x;
        reverbR[(size_t) i] = x;
    }

    reverb.processStereo (reverbL.data(), reverbR.data(), numSamples);

    // Drive on the reverb only (rumble), then low-pass again to keep the dirt in the low end
    const auto driveAmount = std::clamp (values[mst::revDrive], 0.0f, 1.0f);
    if (driveAmount > 0.0001f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            reverbL[(size_t) i] = reverbPostLpL.process (reverbDriveL.process (reverbL[(size_t) i] * 3.0f, driveAmount)).lp;
            reverbR[(size_t) i] = reverbPostLpR.process (reverbDriveR.process (reverbR[(size_t) i] * 3.0f, driveAmount)).lp;
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        reverbL[(size_t) i] *= duckGain[(size_t) i];
        reverbR[(size_t) i] *= duckGain[(size_t) i];
    }
}

void MasterFx::computeDucking (const float* left, const float* right, int numSamples) noexcept
{
    const auto depth   = std::clamp (values[mst::revDuck], 0.0f, 1.0f);
    const auto attack  = std::exp (-1.0f / (0.002f * (float) sampleRate));
    const auto release = std::exp (-1.0f / (0.12f  * (float) sampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto level = std::max (std::abs (left[i]), std::abs (right[i]));
        const auto coeff = level > duckEnvelope ? attack : release;
        duckEnvelope = coeff * duckEnvelope + (1.0f - coeff) * level;
        duckGain[(size_t) i] = 1.0f - depth * std::min (1.0f, duckEnvelope * 3.0f);
    }
}

void MasterFx::processDistortion (float* left, float* right, int numSamples) noexcept
{
    const auto amount = std::clamp (values[mst::distDrive], 0.0f, 1.0f);
    if (amount <= 0.0001f)
        return;

    const auto symmetry = std::clamp (values[mst::distSym], -1.0f, 1.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        left[i]  = toneL.process (distL.process (left[i],  amount, symmetry));
        right[i] = toneR.process (distR.process (right[i], amount, symmetry));
    }
}

void MasterFx::processCompressor (float* left, float* right, int numSamples) noexcept
{
    const auto mix = std::clamp (values[mst::compMix], 0.0f, 1.0f);
    if (mix <= 0.0001f)
    {
        gainReductionDb = 0.0f;
        return;
    }

    const auto threshold = values[mst::compThreshold];
    const auto slope     = 1.0f - 1.0f / std::max (1.0f, values[mst::compRatio]);
    const auto makeup    = values[mst::compMakeup];
    const auto attack    = std::exp (-1.0f / (std::max (values[mst::compAttack], 0.00005f) * (float) sampleRate));
    const auto release   = std::exp (-1.0f / (std::max (values[mst::compRelease], 0.005f) * (float) sampleRate));
    const bool useSidechainFilter = values[mst::compScHpf] > 21.0f;
    constexpr float knee = 6.0f;

    float maxReduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        auto scL = left[i], scR = right[i];
        if (useSidechainFilter)
        {
            scL = sidechainL.process (scL).hp;
            scR = sidechainR.process (scR).hp;
        }

        const auto level = std::max (std::abs (scL), std::abs (scR));
        const auto levelDb = 20.0f * std::log10 (std::max (level, 1.0e-6f));
        const auto over = levelDb - threshold;

        float reduction = 0.0f;
        if (over >= knee * 0.5f)       reduction = over * slope;
        else if (over > -knee * 0.5f)  reduction = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

        const auto coeff = reduction > compEnvelope ? attack : release;
        compEnvelope = coeff * compEnvelope + (1.0f - coeff) * reduction;

        const auto gain = std::pow (10.0f, (makeup - compEnvelope) / 20.0f);
        const auto wet  = 1.0f - mix + mix * gain;

        left[i]  *= wet;
        right[i] *= wet;
        maxReduction = std::max (maxReduction, compEnvelope);
    }

    gainReductionDb = maxReduction;
}

} // namespace takt
