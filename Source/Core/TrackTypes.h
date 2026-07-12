#pragma once

#include <JuceHeader.h>

using TrackId = juce::Uuid;

enum class TrackType
{
    Audio,
    Midi
};

struct TrackState
{
    TrackId id;
    juce::String name;
    float gain = 0.8f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    bool armed = false;
};
