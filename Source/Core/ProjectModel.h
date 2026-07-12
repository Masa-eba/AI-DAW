#pragma once

#include "../Audio/AudioTrack.h"
#include "../Midi/MidiTrack.h"
#include "TempoMap.h"

#include <JuceHeader.h>

#include <memory>
#include <vector>

class ProjectModel final
{
public:
    TrackId addAudioTrack();
    TrackId addMidiTrack();
    bool removeTrack(const TrackId& trackId);

    AudioTrack* findAudioTrack(const TrackId& trackId);
    MidiTrack* findMidiTrack(const TrackId& trackId);
    const AudioTrack* findAudioTrack(const TrackId& trackId) const;
    const MidiTrack* findMidiTrack(const TrackId& trackId) const;

    std::vector<std::unique_ptr<AudioTrack>>& getAudioTracks();
    std::vector<std::unique_ptr<MidiTrack>>& getMidiTracks();
    const std::vector<std::unique_ptr<AudioTrack>>& getAudioTracks() const;
    const std::vector<std::unique_ptr<MidiTrack>>& getMidiTracks() const;

    void setBpm(double newBpm);
    double getBpm() const;
    TempoMap& getTempoMap();
    const TempoMap& getTempoMap() const;

    double getProjectLengthSeconds() const;
    bool saveToFile(const juce::File& file) const;
    bool loadFromFile(const juce::File& file);
    void clearProject();

private:
    std::vector<std::unique_ptr<AudioTrack>> audioTracks;
    std::vector<std::unique_ptr<MidiTrack>> midiTracks;
    TempoMap tempoMap;
};
