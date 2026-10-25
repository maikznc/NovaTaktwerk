#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace takt::dsp
{

inline constexpr float pi    = 3.14159265358979323846f;
inline constexpr float twoPi = 2.0f * pi;

inline float semitonesToRatio (float semis) noexcept { return std::exp2 (semis / 12.0f); }

/** Fast white noise (xorshift32), output in -1..1. */
struct Noise
{
    uint32_t state = 0x9E3779B9u;

    void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9E3779B9u; }

    float next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

/** Exponential decay envelope. "seconds" is the time to fall by 60 dB. */
struct DecayEnv
{
    float value = 0.0f;
    float coeff = 0.0f;

    void trigger (float start, float seconds, double sampleRate) noexcept
    {
        value = start;
        coeff = seconds <= 0.0f ? 0.0f
                                : std::exp (-6.907755f / (seconds * (float) sampleRate));
    }

    float next() noexcept
    {
        const auto out = value;
        value *= coeff;
        return out;
    }

    bool isSilent (float threshold = 1.0e-4f) const noexcept { return value < threshold; }
    void kill() noexcept { value = 0.0f; }
};

/** Band-limited square wave (PolyBLEP), output in -1..1. */
struct BlepSquare
{
    double phase = 0.0;
    double inc   = 0.0;

    void setFrequency (double hz, double sampleRate) noexcept { inc = std::clamp (hz / sampleRate, 0.0, 0.45); }
    void reset (double startPhase = 0.0) noexcept { phase = startPhase; }

    float next() noexcept
    {
        auto v = phase < 0.5 ? 1.0 : -1.0;
        v += blep (phase);
        auto shifted = phase + 0.5;
        if (shifted >= 1.0) shifted -= 1.0;
        v -= blep (shifted);

        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        return (float) v;
    }

private:
    double blep (double t) const noexcept
    {
        if (t < inc)
        {
            t /= inc;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - inc)
        {
            t = (t - 1.0) / inc;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }
};

/** Sine oscillator with a phase accumulator, frequency can change every sample. */
struct SineOsc
{
    float phase = 0.0f;

    void reset() noexcept { phase = 0.0f; }

    float next (float hz, double sampleRate) noexcept
    {
        const auto out = std::sin (twoPi * phase);
        phase += hz / (float) sampleRate;
        phase -= std::floor (phase);
        return out;
    }
};

/** Topology-preserving-transform state variable filter (2-pole, 12 dB/oct). */
struct Svf
{
    struct Outputs { float lp, bp, hp; };

    void setup (float cutoffHz, float q, double sampleRate) noexcept
    {
        cutoffHz = std::clamp (cutoffHz, 10.0f, 0.49f * (float) sampleRate);
        q = std::max (q, 0.05f);
        g  = std::tan (pi * cutoffHz / (float) sampleRate);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    Outputs process (float v0) noexcept
    {
        const auto v3 = v0 - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }

    float k = 1.0f;

private:
    float g = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

/** One-pole DC blocker (high-pass at a few Hz). */
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;

    void prepare (double sampleRate) noexcept { r = 1.0f - (twoPi * 5.0f / (float) sampleRate); reset(); }
    void reset() noexcept { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        const auto y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

/** One-pole low-pass, used for smoothing and gentle tone controls. */
struct OnePoleLowPass
{
    float a = 1.0f, z = 0.0f;

    void setCutoff (float hz, double sampleRate) noexcept
    {
        a = 1.0f - std::exp (-twoPi * std::min (hz, 0.49f * (float) sampleRate) / (float) sampleRate);
    }

    void reset (float value = 0.0f) noexcept { z = value; }
    float process (float x) noexcept { z += a * (x - z); return z; }
};

/**
    Overdrive: up to ~40x gain into a slightly asymmetric tanh (even harmonics, "analog" grit).
    amount 0..1, symmetry -1..1 (0 = symmetric).
*/
struct Overdrive
{
    DcBlocker dc;

    void prepare (double sampleRate) noexcept { dc.prepare (sampleRate); }
    void reset() noexcept { dc.reset(); }

    float process (float x, float amount, float symmetry = 0.0f) noexcept
    {
        if (amount <= 0.0001f)
            return x;

        const auto gain = 1.0f + amount * amount * 39.0f;
        const auto bias = 0.15f * amount + 0.6f * symmetry;
        auto y = std::tanh (gain * x + bias) - std::tanh (bias);
        y = dc.process (y);

        // fade in the effect over the first few percent, so small amounts stay subtle
        const auto wet = std::min (1.0f, amount * 8.0f);
        return x * (1.0f - wet) + y * wet / (1.0f + 0.25f * amount);
    }
};

/** Linear ramp/exponential envelope: Attack → Hold → Decay (amp page). */
struct AhdEnvelope
{
    enum class Stage { idle, attack, hold, decay, sustainForever };

    void trigger (float attackSec, float holdSec, bool holdForever, float decaySec, bool decayForever, double sampleRate) noexcept
    {
        fs = sampleRate;
        holdSamples   = (int) (holdSec * (float) fs);
        infiniteHold  = holdForever;
        infiniteDecay = decayForever;
        decayCoeff    = std::exp (-6.907755f / (std::max (decaySec, 0.001f) * (float) fs));

        if (attackSec > 0.0005f)
        {
            attackInc = 1.0f / (attackSec * (float) fs);
            value = 0.0f;
            stage = Stage::attack;
        }
        else
        {
            value = 1.0f;
            enterHold();
        }
    }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::idle:   return 0.0f;
            case Stage::attack:
                value += attackInc;
                if (value >= 1.0f) { value = 1.0f; enterHold(); }
                return value;
            case Stage::hold:
                if (--holdCounter <= 0) stage = infiniteDecay ? Stage::sustainForever : Stage::decay;
                return value;
            case Stage::decay:
                value *= decayCoeff;
                if (value < 1.0e-4f) { value = 0.0f; stage = Stage::idle; }
                return value;
            case Stage::sustainForever:
                return value;
        }
        return 0.0f;
    }

    bool isFinished() const noexcept { return stage == Stage::idle; }
    void stop() noexcept { stage = Stage::idle; value = 0.0f; }

private:
    void enterHold() noexcept
    {
        if (infiniteHold)          { stage = Stage::sustainForever; return; }
        holdCounter = holdSamples;
        stage = holdCounter > 0 ? Stage::hold : (infiniteDecay ? Stage::sustainForever : Stage::decay);
    }

    double fs = 48000.0;
    Stage stage = Stage::idle;
    float value = 0.0f, attackInc = 0.0f, decayCoeff = 0.0f;
    int holdSamples = 0, holdCounter = 0;
    bool infiniteHold = true, infiniteDecay = true;
};

/** ADSR for the filter (gate from note-on / note-off). Output 0..1. */
struct AdsrEnvelope
{
    enum class Stage { idle, attack, decay, sustain, release };

    void setup (float attackSec, float decaySec, float sustainLevel, float releaseSec, double sampleRate) noexcept
    {
        attackInc    = attackSec > 0.0005f ? 1.0f / (attackSec * (float) sampleRate) : 2.0f;
        decayCoeff   = std::exp (-6.907755f / (std::max (decaySec, 0.001f) * (float) sampleRate));
        releaseCoeff = std::exp (-6.907755f / (std::max (releaseSec, 0.001f) * (float) sampleRate));
        sustain      = std::clamp (sustainLevel, 0.0f, 1.0f);
    }

    void noteOn() noexcept  { stage = Stage::attack; }
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::idle:    break;
            case Stage::attack:
                value += attackInc;
                if (value >= 1.0f) { value = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                value = sustain + (value - sustain) * decayCoeff;
                if (value - sustain < 1.0e-4f) { value = sustain; stage = Stage::sustain; }
                break;
            case Stage::sustain: value = sustain; break;
            case Stage::release:
                value *= releaseCoeff;
                if (value < 1.0e-4f) { value = 0.0f; stage = Stage::idle; }
                break;
        }
        return value;
    }

    bool isIdle() const noexcept { return stage == Stage::idle; }
    void reset() noexcept { stage = Stage::idle; value = 0.0f; }

private:
    Stage stage = Stage::idle;
    float value = 0.0f, attackInc = 1.0f, decayCoeff = 0.0f, releaseCoeff = 0.0f, sustain = 0.0f;
};

} // namespace takt::dsp
