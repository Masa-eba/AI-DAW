#include "MidiTrack.h"

MidiTrack::MidiTrack(juce::String trackName)
{
    state.id = TrackId();
    state.name = std::move(trackName);
}
