#pragma once

#include <JuceHeader.h>

struct MidiClip
{
    juce::Uuid id;
    double startBeat = 0.0;
    double lengthBeats = 0.0;
    bool muted = false;
    juce::MidiMessageSequence sequence;
};
