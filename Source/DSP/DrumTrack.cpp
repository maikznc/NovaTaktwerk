#include "DrumTrack.h"

namespace takt
{

using namespace dsp;

namespace
{
    constexpr float envOctaves = 7.0f;
    constexpr float ln2 = 0.69314718f;
}

void DrumTrack::prepare (double newSampleRate) noexcept
{
    sampleRate   = newSampleRate;
    smoothCoeff  = 1.0f - std::exp (-1.0f / (0.01f * (float) sampleRate));
    declickCoeff = std::exp (-1.0f / (0.002f * (float) sampleRate));
    chokeCoeff   = std::exp (-1.0f / (0.004f * (float) sampleRate));

    synth.prepare (sampleRate);
    overdrive.prepare (sampleRate);
    stage1.reset();
    stage2.reset();
    filterEnv.reset();
    ampEnv.stop();

    logCutoff = std::log (std::clamp (params[Page::filter][flt::freq], 20.0f, 20000.0f));
    filterCountdown = 0;
    lastInput = declick = 0.0f;
    chokeGain = 1.0f;
    choking = false;
    idleSamples = 0;

    // start the smoothed gains at their target, so the very first hit isn't faded in
    const auto& a = params[Page::amp];
    const auto angle = (std::clamp (a[amp::pan], -1.0f, 1.0f) + 1.0f) * pi * 0.25f;
    const auto level = a[amp::volume] * a[amp::volume];
    gainL = std::cos (angle) * level;
    gainR = std::sin (angle) * level;
    sendDelay  = a[amp::delay]  * level;
    sendReverb = a[amp::reverb] * level;
}

void DrumTrack::trigger (float velocity) noexcept
{
    declick   = lastInput;   // removes the jump from whatever was still sounding
    chokeGain = 1.0f;
    choking   = false;
    idleSamples = 0;

    const auto& a = params[Page::amp];
    const auto& f = params[Page::filter];
    const auto& ampD = ampDefs();

    ampEnv.trigger (a[amp::attack],
                    a[amp::hold],  a[amp::hold]  >= ampD[amp::hold].max  - 0.0001f,
                    a[amp::decay], a[amp::decay] >= ampD[amp::decay].max - 0.0001f,
                    sampleRate);

    filterEnv.setup (f[flt::attack], f[flt::decay], f[flt::sustain], f[flt::release], sampleRate);
    filterEnv.noteOn();
    filterCountdown = 0;

    synth.trigger (velocity, params[Page::synth]);
    player.trigger (velocity, params[Page::sample], sampleRate);
}

void DrumTrack::choke() noexcept
{
    if (! ampEnv.isFinished())
        choking = true;
}

void DrumTrack::stopSound() noexcept
{
    synth.stop();
    player.stop();
    ampEnv.stop();
    lastInput = 0.0f;
}

float DrumTrack::applyFilter (float x, int type) noexcept
{
    switch ((FilterType) type)
    {
        case FilterType::lp24: return stage2.process (stage1.process (x).lp).lp;
        case FilterType::lp12: return stage1.process (x).lp;
        case FilterType::bp:   return stage1.process (x).bp * stage1.k;
        case FilterType::hp12: return stage1.process (x).hp;
        case FilterType::hp24: return stage2.process (stage1.process (x).hp).hp;
        case FilterType::bs:   { const auto o = stage1.process (x); return o.lp + o.hp; }
        case FilterType::pk:   { const auto o = stage1.process (x); return x + o.bp * stage1.k * peakBoost; }
    }
    return x;
}

void DrumTrack::render (float* left, float* right, float* delaySend, float* reverbSend, int numSamples) noexcept
{
    const auto& f = params[Page::filter];
    const auto& a = params[Page::amp];

    const auto synthLevel  = params[Page::synth][syn::level];
    const auto sampleLevel = params[Page::sample][smp::level];

    const auto targetLog = std::log (std::clamp (f[flt::freq], 20.0f, 20000.0f));
    const auto envAmount = std::clamp (f[flt::env], -1.0f, 1.0f) * envOctaves * ln2;
    const auto reso      = std::clamp (f[flt::reso], 0.0f, 1.0f);
    const auto type      = std::clamp ((int) std::lround (f[flt::type]), 0, 6);
    resoQ     = 0.6f + reso * reso * 14.0f;
    peakBoost = 1.0f + reso * 3.0f;

    const auto driveAmount = a[amp::drive];
    const auto angle   = (std::clamp (a[amp::pan], -1.0f, 1.0f) + 1.0f) * pi * 0.25f;
    const auto level   = a[amp::volume] * a[amp::volume];
    const auto targetL = std::cos (angle) * level;
    const auto targetR = std::sin (angle) * level;
    const auto targetDelay  = a[amp::delay]  * level;
    const auto targetReverb = a[amp::reverb] * level;

    for (int i = 0; i < numSamples; ++i)
    {
        const bool sourcesActive = synth.isActive() || player.isPlaying();

        if (! sourcesActive && std::abs (declick) < 1.0e-6f)
        {
            if (idleSamples > 4096)
            {
                // Fully silent: keep the smoothers in sync and skip the work.
                logCutoff = targetLog;
                gainL = targetL; gainR = targetR;
                sendDelay = targetDelay; sendReverb = targetReverb;
                lastInput = 0.0f;
                stage1.reset(); stage2.reset(); overdrive.reset();
                return;
            }
            ++idleSamples;
        }
        else
        {
            idleSamples = 0;
        }

        auto input = synth.process() * synthLevel + player.next() * sampleLevel;
        lastInput = input;
        input += declick;
        declick *= declickCoeff;

        // filter (cutoff smoothed, envelope in octaves)
        const auto envValue = filterEnv.next();
        logCutoff += (targetLog - logCutoff) * smoothCoeff;
        if (--filterCountdown <= 0)
        {
            const auto cutoff = std::exp (logCutoff + envAmount * envValue);
            const auto q = (type == (int) FilterType::lp24 || type == (int) FilterType::hp24) ? resoQ * 0.9f : resoQ;
            stage1.setup (cutoff, q, sampleRate);
            stage2.setup (cutoff, 0.6f, sampleRate);
            filterCountdown = 8;
        }

        auto out = applyFilter (input, type);
        out = overdrive.process (out, driveAmount);
        out *= ampEnv.next();

        if (ampEnv.isFinished() && sourcesActive)
            stopSound();

        if (choking)
        {
            chokeGain *= chokeCoeff;
            if (chokeGain < 1.0e-4f)
            {
                chokeGain = 0.0f;
                choking = false;
                stopSound();
            }
        }
        out *= chokeGain;

        gainL      += (targetL - gainL) * smoothCoeff;
        gainR      += (targetR - gainR) * smoothCoeff;
        sendDelay  += (targetDelay - sendDelay) * smoothCoeff;
        sendReverb += (targetReverb - sendReverb) * smoothCoeff;

        left[i]       += out * gainL;
        right[i]      += out * gainR;
        delaySend[i]  += out * sendDelay;
        reverbSend[i] += out * sendReverb;

        peak = std::max (peak, std::abs (out) * level);
    }
}

} // namespace takt
