#include "AudioTrack.h"

AudioTrack::AudioTrack(juce::String trackName)
{
    state.id = TrackId();
    state.name = std::move(trackName);
}

double AudioTrack::getLengthSeconds() const
{
    if (sampleRate <= 0.0 || audioBuffer.getNumSamples() <= 0)
        return 0.0;

    auto length = getAudioDurationSeconds();

    for (const auto& clip : clips)
        length = juce::jmax(length, clip.startTimeSeconds + clip.lengthSeconds);

    return length;
}

double AudioTrack::getAudioDurationSeconds() const
{
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(audioBuffer.getNumSamples()) / sampleRate;
}

bool AudioTrack::hasAudio() const
{
    return sampleRate > 0.0 && audioBuffer.getNumSamples() > 0;
}
