#include "DrumSynth.h"

namespace takt
{

using namespace dsp;

namespace
{
    // Six square oscillators for hats and cymbal (classic metallic cluster).
    constexpr std::array<float, 6> metalFreqs { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

    float clamp01 (float v) noexcept { return std::clamp (v, 0.0f, 1.0f); }
}

void DrumSynth::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    active = false;
    filterA.reset();
    filterB.reset();
}

float DrumSynth::pitchFactor() noexcept
{
    const auto p = pitch;
    pitch *= pitchCoeff;
    return std::exp2 (sweepOctaves * p);
}

void DrumSynth::trigger (float velocity, const PageValues& p) noexcept
{
    const auto fs    = sampleRate;
    const auto ratio = semitonesToRatio (p[syn::tune]);
    const auto decay = std::max (p[syn::decay], 0.005f);

    amp    = clamp01 (velocity);
    active = true;
    holdCounter = 0;
    pitch = 1.0f;
    sweepOctaves = 0.0f;

    osc1.reset(); osc2.reset(); osc3.reset();
    filterA.reset(); filterB.reset();
    ampEnv.kill(); noiseEnv.kill(); clickEnv.kill(); blipEnv.kill(); auxEnv.kill();

    auto setSweep = [&] (float semitones, float tauSeconds)
    {
        sweepOctaves = std::max (0.0f, semitones) / 12.0f;
        pitchCoeff   = std::exp (-1.0f / (std::max (tauSeconds, 0.0005f) * (float) fs));
    };

    switch (track.type)
    {
        case VoiceType::kick:
        case VoiceType::tom:
        {
            freq = track.baseFreq * ratio;
            setSweep (p[tonal::sweep], p[tonal::sweepTime] / 3.0f);   // ~95 % of the sweep is done after Sw.Time
            holdCounter = (int) (p[tonal::hold] * (float) fs);
            ampEnv.trigger (1.0f, decay, fs);
            auxEnv.trigger (track.type == VoiceType::tom ? 0.28f : 0.0f, decay * 0.45f, fs);   // tom overtone
            waveDrive = 1.0f + clamp01 (p[tonal::wave]) * 14.0f;
            snapLevel = clamp01 (p[tonal::snap]);
            clickEnv.trigger (1.0f, 0.005f, fs);
            blipEnv.trigger (1.0f, 0.009f, fs);
            filterA.setup (1800.0f, 0.7f, fs);
            break;
        }

        case VoiceType::snare:
            freq  = track.baseFreq * ratio;
            freq2 = freq * semitonesToRatio (p[snr::detune]);
            setSweep (p[snr::sweep], 0.012f);
            ampEnv.trigger   (1.0f, decay, fs);
            noiseEnv.trigger (1.0f, std::max (p[snr::noiseDecay], 0.005f), fs);
            snapLevel = clamp01 (p[snr::snap]);
            filterA.setup (500.0f  + p[snr::noiseTone] * 5500.0f,  0.7f, fs);   // noise high-pass
            filterB.setup (3000.0f + p[snr::noiseTone] * 15000.0f, 0.7f, fs);   // noise low-pass
            break;

        case VoiceType::rim:
            freq    = track.baseFreq * ratio;
            freq2   = freq * semitonesToRatio (p[rim::interval]);
            balance = clamp01 (p[rim::balance]);
            snapLevel = clamp01 (p[rim::snap]);
            setSweep (p[rim::sweep], 0.004f);
            ampEnv.trigger   (1.0f, decay, fs);
            clickEnv.trigger (1.0f, 0.003f, fs);
            filterA.setup (p[rim::cut], 0.7f, fs);
            break;

        case VoiceType::clap:
            filterA.setup (1200.0f * ratio, 0.7f + clamp01 (p[clp::tone]) * 5.0f, fs);
            filterB.setup (500.0f, 0.7f, fs);
            clapBurstsLeft = std::max (0, (int) std::lround (p[clp::claps]) - 1);
            clapSpacing    = std::max (1, (int) (p[clp::rate] * (float) fs));
            clapCounter    = 0;
            tailSeconds    = decay;
            burstLevel     = 0.55f + clamp01 (p[clp::snap]) * 0.6f;
            bodyLevel      = clamp01 (p[clp::body]);
            auxEnv.trigger (1.0f, 0.03f, fs);
            if (clapBurstsLeft == 0)
                noiseEnv.trigger (0.75f, tailSeconds, fs);
            break;

        case VoiceType::closedHat:
        case VoiceType::openHat:
        case VoiceType::cymbal:
        {
            for (size_t i = 0; i < squares.size(); ++i)
            {
                squares[i].setFrequency (metalFreqs[i] * ratio, fs);
                squares[i].reset ((double) i * 0.13);
            }

            const auto color = clamp01 (p[metal::color]);
            filterA.setup (p[metal::tone], 0.6f + color * 8.0f, fs);
            filterB.setup (p[metal::cut], 0.7f, fs);
            noiseLevel = clamp01 (p[metal::noise]);
            snapLevel  = clamp01 (p[metal::snap]);
            gain = (track.type == VoiceType::cymbal ? 2.6f : 3.4f) * (1.0f + color * 1.2f);
            ampEnv.trigger (1.0f, decay, fs);
            clickEnv.trigger (1.0f, 0.006f, fs);
            break;
        }

        case VoiceType::cowbell:
            freq  = track.baseFreq * ratio;
            freq2 = freq * semitonesToRatio (p[bell::interval]);
            squares[0].setFrequency (freq, fs);
            squares[1].setFrequency (freq2, fs);
            squares[0].reset();
            squares[1].reset (0.25);
            filterA.setup (p[bell::tone], 0.8f + clamp01 (p[bell::color]) * 5.0f, fs);
            snapLevel   = clamp01 (p[bell::snap]);
            holdCounter = (int) (p[bell::hold] * (float) fs);
            auxEnv.trigger (1.0f, 0.04f, fs);
            ampEnv.trigger (0.55f, decay, fs);
            break;
    }
}

float DrumSynth::process() noexcept
{
    if (! active)
        return 0.0f;

    switch (track.type)
    {
        case VoiceType::kick:
        case VoiceType::tom:       return processTonal();
        case VoiceType::snare:     return processSnare();
        case VoiceType::rim:       return processRim();
        case VoiceType::clap:      return processClap();
        case VoiceType::closedHat:
        case VoiceType::openHat:
        case VoiceType::cymbal:    return processMetal();
        case VoiceType::cowbell:   return processCowbell();
    }
    return 0.0f;
}

//==============================================================================
float DrumSynth::processTonal() noexcept
{
    const auto f = std::min (freq * pitchFactor(), 0.45f * (float) sampleRate);
    auto s = osc1.next (f, sampleRate);

    // Wave: drive the sine into a tanh. Less drive while the pitch is still high, so the sweep doesn't alias.
    if (waveDrive > 1.001f)
    {
        const auto k = 1.0f + (waveDrive - 1.0f) * std::clamp (400.0f / f, 0.15f, 1.0f);
        s = std::tanh (k * s) / std::tanh (k);
    }

    float env;
    if (holdCounter > 0) { --holdCounter; env = ampEnv.value; }
    else                 { env = ampEnv.next(); }

    const auto overtone = osc2.next (f * 1.51f, sampleRate) * auxEnv.next();

    const auto click = filterA.process (noise.next()).hp * clickEnv.next() * 0.8f
                     + osc3.next (1600.0f, sampleRate) * blipEnv.next() * 0.45f;

    active = holdCounter > 0 || ! ampEnv.isSilent();
    return (s * env * 0.95f + overtone + click * snapLevel) * amp;
}

float DrumSynth::processSnare() noexcept
{
    const auto pf   = pitchFactor();
    const auto body = (osc1.next (freq * pf, sampleRate) * 0.6f + osc2.next (freq2 * pf, sampleRate) * 0.4f)
                    * ampEnv.next() * (1.0f - snapLevel * 0.35f);

    auto n = filterA.process (noise.next()).hp;
    n = filterB.process (n).lp * noiseEnv.next() * snapLevel;

    active = ! (ampEnv.isSilent() && noiseEnv.isSilent());
    const auto x = body * 0.9f + n * 1.1f;
    return std::tanh (1.4f * x) / std::tanh (1.4f) * amp;
}

float DrumSynth::processRim() noexcept
{
    const auto pf = pitchFactor();
    auto s = (osc1.next (freq * pf, sampleRate) * (1.0f - balance) + osc2.next (freq2 * pf, sampleRate) * balance)
             * ampEnv.next() * 1.3f;
    s += noise.next() * clickEnv.next() * snapLevel * 0.7f;
    s = filterA.process (s).hp;

    active = ! ampEnv.isSilent();
    return std::tanh (2.2f * s) / std::tanh (2.2f) * amp * 0.9f;
}

float DrumSynth::processClap() noexcept
{
    if (clapBurstsLeft > 0 && ++clapCounter >= clapSpacing)
    {
        clapCounter = 0;
        --clapBurstsLeft;
        auxEnv.trigger (1.0f, 0.03f, sampleRate);

        if (clapBurstsLeft == 0)
            noiseEnv.trigger (0.75f, tailSeconds, sampleRate);
    }

    const auto e = std::max (auxEnv.next() * burstLevel, noiseEnv.next());
    const auto n = noise.next();
    const auto band = filterA.process (n).bp * filterA.k;
    const auto low  = filterB.process (n).lp;

    active = clapBurstsLeft > 0 || ! (auxEnv.isSilent() && noiseEnv.isSilent());
    return (band * 3.2f + low * bodyLevel * 2.0f) * e * amp;
}

float DrumSynth::processMetal() noexcept
{
    float m = 0.0f;
    for (auto& s : squares)
        m += s.next();

    const auto n = noise.next();
    m = m * (1.0f / 6.0f) * (1.0f - noiseLevel * 0.8f) + n * noiseLevel;

    const auto band  = filterA.process (m).bp * filterA.k;
    const auto burst = n * clickEnv.next() * snapLevel * 1.5f;
    const auto high  = filterB.process (band + burst).hp;
    const auto e     = ampEnv.next();

    active = ! ampEnv.isSilent();

    if (track.type == VoiceType::cymbal)
        return (high + band * 0.3f) * gain * e * amp;

    return high * gain * e * amp;
}

float DrumSynth::processCowbell() noexcept
{
    const auto raw  = squares[0].next() * 0.5f + squares[1].next() * 0.5f;
    const auto band = filterA.process (raw).bp * filterA.k;

    float main;
    if (holdCounter > 0) { --holdCounter; main = ampEnv.value; }
    else                 { main = ampEnv.next(); }

    const auto e = auxEnv.next() * snapLevel * 0.8f + main;

    active = holdCounter > 0 || ! ampEnv.isSilent();
    return (band * 0.9f + raw * 0.12f) * e * amp;
}

} // namespace takt
