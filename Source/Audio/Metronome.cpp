#include "Metronome.h"

#include <cmath>

void Metronome::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
}

void Metronome::setBpm(double newBpm)
{
    bpm = juce::jlimit(20.0, 300.0, newBpm);
}

void Metronome::setEnabled(bool shouldBeEnabled)
{
    enabled = shouldBeEnabled;
}

bool Metronome::isEnabled() const
{
    return enabled;
}

void Metronome::setGain(float newGain)
{
    gain = juce::jlimit(0.0f, 1.0f, newGain);
}

float Metronome::getGain() const
{
    return gain;
}

void Metronome::reset(double positionSeconds)
{
    const auto secondsPerBeat = 60.0 / bpm;
    beatIndex = static_cast<int>(std::floor(juce::jmax(0.0, positionSeconds) / secondsPerBeat));
    nextBeatTimeSeconds = static_cast<double>(beatIndex) * secondsPerBeat;

    if (nextBeatTimeSeconds < positionSeconds)
    {
        ++beatIndex;
        nextBeatTimeSeconds += secondsPerBeat;
    }
}

void Metronome::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                int startSample,
                                int numSamples,
                                double blockStartTimeSeconds)
{
    if (! enabled || bpm <= 0.0)
        return;

    const auto blockEndTime = blockStartTimeSeconds
                            + static_cast<double>(numSamples) / sampleRate;
    const auto secondsPerBeat = 60.0 / bpm;

    while (nextBeatTimeSeconds >= blockStartTimeSeconds && nextBeatTimeSeconds < blockEndTime)
    {
        const auto offset = static_cast<int>((nextBeatTimeSeconds - blockStartTimeSeconds) * sampleRate);
        renderClick(buffer, startSample + juce::jlimit(0, numSamples - 1, offset), beatIndex % 4 == 0);
        ++beatIndex;
        nextBeatTimeSeconds += secondsPerBeat;
    }

    while (nextBeatTimeSeconds < blockStartTimeSeconds)
    {
        ++beatIndex;
        nextBeatTimeSeconds += secondsPerBeat;
    }
}

void Metronome::renderClick(juce::AudioBuffer<float>& buffer, int sampleOffset, bool downbeat)
{
    const auto frequency = downbeat ? 1500.0 : 1000.0;
    const auto amplitude = (downbeat ? 0.45f : 0.25f) * gain;
    const auto durationSamples = static_cast<int>(sampleRate * 0.035);

    for (auto sample = 0; sample < durationSamples; ++sample)
    {
        const auto target = sampleOffset + sample;

        if (target >= buffer.getNumSamples())
            break;

        const auto envelope = 1.0f - static_cast<float>(sample) / static_cast<float>(durationSamples);
        const auto value = amplitude * envelope
                         * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi
                                                       * frequency
                                                       * static_cast<double>(sample)
                                                       / sampleRate));

        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.addSample(channel, target, value);
    }
}
