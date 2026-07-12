#pragma once

#include "../Core/TrackTypes.h"
#include "AudioClip.h"

#include <JuceHeader.h>

#include <vector>

class AudioTrack final
{
public:
    explicit AudioTrack(juce::String trackName);

    TrackState state;
    std::vector<AudioClip> clips;
    juce::AudioBuffer<float> audioBuffer;
    double sampleRate = 0.0;

    double getLengthSeconds() const;
    double getAudioDurationSeconds() const;
    bool hasAudio() const;
};
