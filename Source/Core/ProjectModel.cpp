#include "ProjectModel.h"

TrackId ProjectModel::addAudioTrack()
{
    auto track = std::make_unique<AudioTrack>("Audio " + juce::String(audioTracks.size() + 1));
    const auto id = track->state.id;
    audioTracks.push_back(std::move(track));
    return id;
}

TrackId ProjectModel::addMidiTrack()
{
    auto track = std::make_unique<MidiTrack>("MIDI " + juce::String(midiTracks.size() + 1));
    const auto id = track->state.id;
    midiTracks.push_back(std::move(track));
    return id;
}

bool ProjectModel::removeTrack(const TrackId& trackId)
{
    const auto removeAudio = std::remove_if(audioTracks.begin(), audioTracks.end(),
                                            [&trackId](const auto& track)
                                            {
                                                return track->state.id == trackId;
                                            });

    if (removeAudio != audioTracks.end())
    {
        audioTracks.erase(removeAudio, audioTracks.end());
        return true;
    }

    const auto removeMidi = std::remove_if(midiTracks.begin(), midiTracks.end(),
                                           [&trackId](const auto& track)
                                           {
                                               return track->state.id == trackId;
                                           });

    if (removeMidi != midiTracks.end())
    {
        midiTracks.erase(removeMidi, midiTracks.end());
        return true;
    }

    return false;
}

AudioTrack* ProjectModel::findAudioTrack(const TrackId& trackId)
{
    return const_cast<AudioTrack*>(std::as_const(*this).findAudioTrack(trackId));
}

MidiTrack* ProjectModel::findMidiTrack(const TrackId& trackId)
{
    return const_cast<MidiTrack*>(std::as_const(*this).findMidiTrack(trackId));
}

const AudioTrack* ProjectModel::findAudioTrack(const TrackId& trackId) const
{
    for (const auto& track : audioTracks)
        if (track->state.id == trackId)
            return track.get();

    return nullptr;
}

const MidiTrack* ProjectModel::findMidiTrack(const TrackId& trackId) const
{
    for (const auto& track : midiTracks)
        if (track->state.id == trackId)
            return track.get();

    return nullptr;
}

std::vector<std::unique_ptr<AudioTrack>>& ProjectModel::getAudioTracks()
{
    return audioTracks;
}

std::vector<std::unique_ptr<MidiTrack>>& ProjectModel::getMidiTracks()
{
    return midiTracks;
}

const std::vector<std::unique_ptr<AudioTrack>>& ProjectModel::getAudioTracks() const
{
    return audioTracks;
}

const std::vector<std::unique_ptr<MidiTrack>>& ProjectModel::getMidiTracks() const
{
    return midiTracks;
}

void ProjectModel::setBpm(double newBpm)
{
    tempoMap.setBpm(newBpm);
}

double ProjectModel::getBpm() const
{
    return tempoMap.getBpm();
}

TempoMap& ProjectModel::getTempoMap()
{
    return tempoMap;
}

const TempoMap& ProjectModel::getTempoMap() const
{
    return tempoMap;
}

double ProjectModel::getProjectLengthSeconds() const
{
    double length = 0.0;

    for (const auto& track : audioTracks)
        length = juce::jmax(length, track->getLengthSeconds());

    for (const auto& track : midiTracks)
        for (const auto& clip : track->clips)
            length = juce::jmax(length, tempoMap.beatsToSeconds(clip.startBeat + clip.lengthBeats));

    return length;
}

bool ProjectModel::saveToFile(const juce::File& file) const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("version", 1);
    root->setProperty("bpm", tempoMap.getBpm());

    juce::Array<juce::var> audio;
    for (const auto& track : audioTracks)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", track->state.id.toString());
        object->setProperty("name", track->state.name);
        object->setProperty("gain", track->state.gain);
        object->setProperty("muted", track->state.muted);
        object->setProperty("solo", track->state.solo);
        object->setProperty("armed", track->state.armed);

        if (! track->clips.empty())
        {
            object->setProperty("file", track->clips.front().sourceFile.getFullPathName());
            object->setProperty("startTimeSeconds", track->clips.front().startTimeSeconds);
            object->setProperty("lengthSeconds", track->clips.front().lengthSeconds);
        }

        audio.add(juce::var(object.release()));
    }
    root->setProperty("audioTracks", audio);

    juce::Array<juce::var> midi;
    for (const auto& track : midiTracks)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", track->state.id.toString());
        object->setProperty("name", track->state.name);
        object->setProperty("gain", track->state.gain);
        object->setProperty("muted", track->state.muted);
        object->setProperty("solo", track->state.solo);
        object->setProperty("armed", track->state.armed);
        midi.add(juce::var(object.release()));
    }
    root->setProperty("midiTracks", midi);

    return file.replaceWithText(juce::JSON::toString(juce::var(root.release()), true));
}

bool ProjectModel::loadFromFile(const juce::File& file)
{
    const auto parsed = juce::JSON::parse(file);

    if (! parsed.isObject())
        return false;

    clearProject();
    tempoMap.setBpm(static_cast<double>(parsed.getProperty("bpm", 120.0)));

    if (auto* audio = parsed.getProperty("audioTracks", {}).getArray())
        for (const auto& item : *audio)
            if (item.isObject())
                addAudioTrack();

    if (auto* midi = parsed.getProperty("midiTracks", {}).getArray())
        for (const auto& item : *midi)
            if (item.isObject())
                addMidiTrack();

    return true;
}

void ProjectModel::clearProject()
{
    audioTracks.clear();
    midiTracks.clear();
    tempoMap.setBpm(120.0);
}
