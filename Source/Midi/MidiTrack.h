#pragma once

#include "../Core/TrackTypes.h"
#include "MidiClip.h"

#include <JuceHeader.h>

#include <vector>

class MidiTrack final
{
public:
    explicit MidiTrack(juce::String trackName);

    TrackState state;
    std::vector<MidiClip> clips;
};
