#include "ProjectModel.h"

#include <cmath>
#include <limits>

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

TrackId ProjectModel::duplicateTrack(const TrackId& trackId)
{
    if (const auto* source = findAudioTrack(trackId))
    {
        auto duplicate = std::make_unique<AudioTrack>(source->state.name + " Copy");
        duplicate->state.gain = source->state.gain;
        duplicate->state.pan = source->state.pan;
        duplicate->state.muted = source->state.muted;
        duplicate->state.solo = false;
        duplicate->state.armed = false;
        duplicate->clips = source->clips;
        duplicate->audioBuffer = source->audioBuffer;
        duplicate->sampleRate = source->sampleRate;

        for (auto& clip : duplicate->clips)
            clip.id = juce::Uuid();

        const auto id = duplicate->state.id;
        audioTracks.push_back(std::move(duplicate));
        return id;
    }

    if (const auto* source = findMidiTrack(trackId))
    {
        auto duplicate = std::make_unique<MidiTrack>(source->state.name + " Copy");
        duplicate->state.gain = source->state.gain;
        duplicate->state.pan = source->state.pan;
        duplicate->state.muted = source->state.muted;
        duplicate->state.solo = false;
        duplicate->state.armed = false;
        duplicate->clips = source->clips;

        for (auto& clip : duplicate->clips)
            clip.id = juce::Uuid();

        const auto id = duplicate->state.id;
        midiTracks.push_back(std::move(duplicate));
        return id;
    }

    return {};
}

bool ProjectModel::moveTrack(const TrackId& trackId, int direction)
{
    if (direction == 0)
        return false;

    const auto moveInList = [&trackId, direction](auto& tracks)
    {
        const auto current = std::find_if(tracks.begin(), tracks.end(),
                                          [&trackId](const auto& track)
                                          {
                                              return track->state.id == trackId;
                                          });

        if (current == tracks.end())
            return false;

        const auto currentIndex = std::distance(tracks.begin(), current);
        const auto targetIndex = currentIndex + (direction < 0 ? -1 : 1);

        if (targetIndex < 0 || targetIndex >= static_cast<decltype(targetIndex)>(tracks.size()))
            return false;

        std::iter_swap(current, tracks.begin() + targetIndex);
        return true;
    };

    if (moveInList(audioTracks))
        return true;

    return moveInList(midiTracks);
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

juce::Uuid ProjectModel::addMarker(double timeSeconds)
{
    ProjectMarker marker;
    marker.timeSeconds = juce::jmax(0.0, timeSeconds);
    marker.name = "Marker " + juce::String(markers.size() + 1);
    const auto id = marker.id;
    markers.push_back(marker);

    std::sort(markers.begin(), markers.end(),
              [](const auto& left, const auto& right)
              {
                  return left.timeSeconds < right.timeSeconds;
              });

    return id;
}

bool ProjectModel::removeNearestMarker(double timeSeconds, double thresholdSeconds)
{
    if (markers.empty())
        return false;

    const auto safeTime = juce::jmax(0.0, timeSeconds);
    const auto safeThreshold = juce::jmax(0.0, thresholdSeconds);
    auto best = markers.end();
    auto bestDistance = std::numeric_limits<double>::max();

    for (auto iterator = markers.begin(); iterator != markers.end(); ++iterator)
    {
        const auto distance = std::abs(iterator->timeSeconds - safeTime);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = iterator;
        }
    }

    if (best == markers.end() || bestDistance > safeThreshold)
        return false;

    markers.erase(best);
    return true;
}

const std::vector<ProjectMarker>& ProjectModel::getMarkers() const
{
    return markers;
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

juce::String ProjectModel::toJsonString() const
{
    return toJsonString({});
}

juce::String ProjectModel::toJsonString(const juce::File& projectDirectory) const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("version", 2);
    root->setProperty("bpm", tempoMap.getBpm());

    juce::Array<juce::var> markerArray;
    for (const auto& marker : markers)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", marker.id.toString());
        object->setProperty("timeSeconds", marker.timeSeconds);
        object->setProperty("name", marker.name);
        markerArray.add(juce::var(object.release()));
    }
    root->setProperty("markers", markerArray);

    juce::Array<juce::var> audio;
    for (const auto& track : audioTracks)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", track->state.id.toString());
        object->setProperty("name", track->state.name);
        object->setProperty("gain", track->state.gain);
        object->setProperty("pan", track->state.pan);
        object->setProperty("muted", track->state.muted);
        object->setProperty("solo", track->state.solo);
        object->setProperty("armed", track->state.armed);

        juce::Array<juce::var> clips;
        for (const auto& clip : track->clips)
        {
            auto clipObject = std::make_unique<juce::DynamicObject>();
            auto filePath = clip.sourceFile.getFullPathName();

            if (projectDirectory != juce::File{} && clip.sourceFile.isAChildOf(projectDirectory))
                filePath = clip.sourceFile.getRelativePathFrom(projectDirectory);

            clipObject->setProperty("id", clip.id.toString());
            clipObject->setProperty("file", filePath);
            clipObject->setProperty("startTimeSeconds", clip.startTimeSeconds);
            clipObject->setProperty("sourceOffsetSeconds", clip.sourceOffsetSeconds);
            clipObject->setProperty("lengthSeconds", clip.lengthSeconds);
            clipObject->setProperty("fadeInSeconds", clip.fadeInSeconds);
            clipObject->setProperty("fadeOutSeconds", clip.fadeOutSeconds);
            clipObject->setProperty("gain", clip.gain);
            clipObject->setProperty("muted", clip.muted);
            clips.add(juce::var(clipObject.release()));
        }
        object->setProperty("clips", clips);

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
        object->setProperty("pan", track->state.pan);
        object->setProperty("muted", track->state.muted);
        object->setProperty("solo", track->state.solo);
        object->setProperty("armed", track->state.armed);

        juce::Array<juce::var> clips;
        for (const auto& clip : track->clips)
        {
            auto clipObject = std::make_unique<juce::DynamicObject>();
            clipObject->setProperty("id", clip.id.toString());
            clipObject->setProperty("startBeat", clip.startBeat);
            clipObject->setProperty("lengthBeats", clip.lengthBeats);
            clipObject->setProperty("muted", clip.muted);

            juce::Array<juce::var> events;
            for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
            {
                const auto* event = clip.sequence.getEventPointer(i);

                if (event == nullptr)
                    continue;

                const auto& message = event->message;

                if (! message.isNoteOnOrOff())
                    continue;

                auto eventObject = std::make_unique<juce::DynamicObject>();
                eventObject->setProperty("type", message.isNoteOn() ? "noteOn" : "noteOff");
                eventObject->setProperty("note", message.getNoteNumber());
                eventObject->setProperty("velocity", message.getVelocity());
                eventObject->setProperty("channel", message.getChannel());
                eventObject->setProperty("timeStamp", message.getTimeStamp());
                events.add(juce::var(eventObject.release()));
            }

            clipObject->setProperty("events", events);
            clips.add(juce::var(clipObject.release()));
        }
        object->setProperty("clips", clips);
        midi.add(juce::var(object.release()));
    }
    root->setProperty("midiTracks", midi);

    return juce::JSON::toString(juce::var(root.release()), true);
}

bool ProjectModel::loadFromJsonString(const juce::String& json)
{
    return loadFromJsonString(json, {});
}

bool ProjectModel::loadFromJsonString(const juce::String& json, const juce::File& projectDirectory)
{
    const auto parsed = juce::JSON::parse(json);

    if (! parsed.isObject())
        return false;

    clearProject();
    tempoMap.setBpm(static_cast<double>(parsed.getProperty("bpm", 120.0)));

    if (auto* markerArray = parsed.getProperty("markers", {}).getArray())
    {
        for (const auto& item : *markerArray)
            if (item.isObject())
            {
                ProjectMarker marker;
                marker.id = juce::Uuid(item.getProperty("id", juce::Uuid().toString()).toString());
                marker.timeSeconds = static_cast<double>(item.getProperty("timeSeconds", 0.0));
                marker.name = item.getProperty("name", "Marker").toString();
                markers.push_back(marker);
            }
    }

    if (auto* audio = parsed.getProperty("audioTracks", {}).getArray())
    {
        for (const auto& item : *audio)
            if (item.isObject())
            {
                auto track = std::make_unique<AudioTrack>(item.getProperty("name", "Audio"));
                track->state.id = TrackId(item.getProperty("id", TrackId().toString()).toString());
                track->state.gain = static_cast<float>(static_cast<double>(item.getProperty("gain", 0.8)));
                track->state.pan = static_cast<float>(static_cast<double>(item.getProperty("pan", 0.0)));
                track->state.muted = static_cast<bool>(item.getProperty("muted", false));
                track->state.solo = static_cast<bool>(item.getProperty("solo", false));
                track->state.armed = static_cast<bool>(item.getProperty("armed", false));

                if (auto* clips = item.getProperty("clips", {}).getArray())
                {
                    for (const auto& clipItem : *clips)
                    {
                        if (! clipItem.isObject())
                            continue;

                        AudioClip clip;
                        const auto filePath = clipItem.getProperty("file", "").toString();
                        clip.id = juce::Uuid(clipItem.getProperty("id", juce::Uuid().toString()).toString());
                        clip.sourceFile = juce::File::isAbsolutePath(filePath) || projectDirectory == juce::File{}
                            ? juce::File(filePath)
                            : projectDirectory.getChildFile(filePath);
                        clip.startTimeSeconds = static_cast<double>(clipItem.getProperty("startTimeSeconds", 0.0));
                        clip.sourceOffsetSeconds = static_cast<double>(clipItem.getProperty("sourceOffsetSeconds", 0.0));
                        clip.lengthSeconds = static_cast<double>(clipItem.getProperty("lengthSeconds", 0.0));
                        clip.fadeInSeconds = static_cast<double>(clipItem.getProperty("fadeInSeconds", 0.0));
                        clip.fadeOutSeconds = static_cast<double>(clipItem.getProperty("fadeOutSeconds", 0.0));
                        clip.gain = static_cast<float>(static_cast<double>(clipItem.getProperty("gain", 1.0)));
                        clip.muted = static_cast<bool>(clipItem.getProperty("muted", false));
                        track->clips.push_back(clip);
                    }
                }

                audioTracks.push_back(std::move(track));
            }
    }

    if (auto* midi = parsed.getProperty("midiTracks", {}).getArray())
    {
        for (const auto& item : *midi)
            if (item.isObject())
            {
                auto track = std::make_unique<MidiTrack>(item.getProperty("name", "MIDI"));
                track->state.id = TrackId(item.getProperty("id", TrackId().toString()).toString());
                track->state.gain = static_cast<float>(static_cast<double>(item.getProperty("gain", 0.8)));
                track->state.pan = static_cast<float>(static_cast<double>(item.getProperty("pan", 0.0)));
                track->state.muted = static_cast<bool>(item.getProperty("muted", false));
                track->state.solo = static_cast<bool>(item.getProperty("solo", false));
                track->state.armed = static_cast<bool>(item.getProperty("armed", false));

                if (auto* clips = item.getProperty("clips", {}).getArray())
                {
                    for (const auto& clipItem : *clips)
                    {
                        if (! clipItem.isObject())
                            continue;

                        MidiClip clip;
                        clip.id = juce::Uuid(clipItem.getProperty("id", juce::Uuid().toString()).toString());
                        clip.startBeat = static_cast<double>(clipItem.getProperty("startBeat", 0.0));
                        clip.lengthBeats = static_cast<double>(clipItem.getProperty("lengthBeats", 0.0));
                        clip.muted = static_cast<bool>(clipItem.getProperty("muted", false));

                        if (auto* events = clipItem.getProperty("events", {}).getArray())
                        {
                            for (const auto& eventItem : *events)
                            {
                                if (! eventItem.isObject())
                                    continue;

                                const auto type = eventItem.getProperty("type", "").toString();
                                const auto channel = static_cast<int>(eventItem.getProperty("channel", 1));
                                const auto note = static_cast<int>(eventItem.getProperty("note", 60));
                                const auto velocity = static_cast<float>(static_cast<double>(eventItem.getProperty("velocity", 0.8)));
                                const auto timeStamp = static_cast<double>(eventItem.getProperty("timeStamp", 0.0));
                                auto message = type == "noteOff"
                                    ? juce::MidiMessage::noteOff(channel, note)
                                    : juce::MidiMessage::noteOn(channel, note, velocity);
                                message.setTimeStamp(timeStamp);
                                clip.sequence.addEvent(message);
                            }
                        }

                        clip.sequence.updateMatchedPairs();
                        track->clips.push_back(clip);
                    }
                }

                midiTracks.push_back(std::move(track));
            }
    }

    return true;
}

bool ProjectModel::saveToFile(const juce::File& file) const
{
    return file.replaceWithText(toJsonString(file.getParentDirectory()));
}

bool ProjectModel::loadFromFile(const juce::File& file)
{
    return loadFromJsonString(file.loadFileAsString(), file.getParentDirectory());
}

void ProjectModel::clearProject()
{
    audioTracks.clear();
    midiTracks.clear();
    markers.clear();
    tempoMap.setBpm(120.0);
}
