#pragma once

#include "../ParamDefs.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace takt
{

/** A loaded sample, mixed down to mono. Shared between the message thread and the audio thread. */
struct SampleData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    juce::AudioBuffer<float> audio;   // one channel
    double sourceSampleRate = 44100.0;
    juce::String name;
};

/**
    Plays a SampleData (the SAMPLE page): Tune/Fine, Start/End (End before Start = reversed),
    Loop, plus bit and sample-rate reduction for crunch.
*/
class SamplePlayer
{
public:
    void setSample (SampleData::Ptr newSample) noexcept
    {
        if (newSample.get() != sample.get())
        {
            sample = std::move (newSample);
            playing = false;
        }
    }

    bool hasSample() const noexcept { return sample != nullptr; }

    void trigger (float velocity, const PageValues& p, double hostSampleRate) noexcept
    {
        playing = false;
        if (sample == nullptr || sample->audio.getNumSamples() < 4)
            return;

        const auto last = (double) (sample->audio.getNumSamples() - 2);
        lastPos  = last;
        startPos = std::clamp ((double) p[smp::start], 0.0, 1.0) * last;
        endPos   = std::clamp ((double) p[smp::end],   0.0, 1.0) * last;

        if (std::abs (endPos - startPos) < 2.0)
            return;

        const auto direction = endPos >= startPos ? 1.0 : -1.0;
        const auto semis = (double) p[smp::tune] + (double) p[smp::fine] / 100.0;
        increment = direction * sample->sourceSampleRate / hostSampleRate * std::exp2 (semis / 12.0);

        position   = startPos;
        looping    = p[smp::loop] >= 0.5f;
        bits       = (int) std::lround (p[smp::bits]);
        decimation = std::max (1, (int) std::lround (p[smp::decim]));
        holdCount  = 0;
        heldValue  = 0.0f;
        gain       = velocity;
        playing    = true;
    }

    float next() noexcept
    {
        if (! playing)
            return 0.0f;

        const bool forward = increment > 0.0;
        if ((forward && position >= endPos) || (! forward && position <= endPos))
        {
            if (! looping)
            {
                playing = false;
                return 0.0f;
            }
            position = std::clamp (startPos + (position - endPos), 0.0, lastPos);
        }

        const auto* data  = sample->audio.getReadPointer (0);
        const auto index  = (int) position;
        const auto frac   = (float) (position - (double) index);
        auto value = data[index] + (data[index + 1] - data[index]) * frac;

        // short fade at the end point so cut samples don't click
        if (! looping)
        {
            const auto remaining = (float) std::abs (endPos - position) / (float) std::abs (increment);
            if (remaining < 64.0f)
                value *= remaining / 64.0f;
        }

        position += increment;

        if (decimation > 1)
        {
            if (--holdCount <= 0)
            {
                heldValue = value;
                holdCount = decimation;
            }
            value = heldValue;
        }

        if (bits < 16)
        {
            const auto steps = (float) (1 << (std::max (bits, 2) - 1));
            value = std::round (value * steps) / steps;
        }

        return value * gain;
    }

    bool isPlaying() const noexcept { return playing; }
    void stop() noexcept { playing = false; }

private:
    SampleData::Ptr sample;
    double position = 0.0, increment = 1.0, startPos = 0.0, endPos = 0.0, lastPos = 0.0;
    float gain = 0.0f, heldValue = 0.0f;
    int bits = 16, decimation = 1, holdCount = 0;
    bool playing = false, looping = false;
};

} // namespace takt
