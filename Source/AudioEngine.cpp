#include "AudioEngine.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

namespace
{
juce::File getAvailableCopyDestination(const juce::File& directory, const juce::File& sourceFile)
{
    const auto legalName = juce::File::createLegalFileName(sourceFile.getFileName());
    const auto baseName = sourceFile.getFileNameWithoutExtension();
    const auto extension = sourceFile.getFileExtension();
    auto destination = directory.getChildFile(legalName);
    auto suffix = 1;

    while (destination.existsAsFile() && destination != sourceFile)
    {
        destination = directory.getChildFile(juce::File::createLegalFileName(baseName
                                                                             + "_"
                                                                             + juce::String(suffix++)
                                                                             + extension));
    }

    return destination;
}
}

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    projectModel.addAudioTrack();
    projectModel.addMidiTrack();
    synth.setCurrentPlaybackSampleRate(currentSampleRate.load());
}

AudioEngine::~AudioEngine()
{
    stop();
    recorder.stopRecording();
}

ProjectModel& AudioEngine::getProjectModel()
{
    return projectModel;
}

const ProjectModel& AudioEngine::getProjectModel() const
{
    return projectModel;
}

TrackId AudioEngine::addAudioTrack()
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.addAudioTrack();
}

TrackId AudioEngine::addMidiTrack()
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.addMidiTrack();
}

TrackId AudioEngine::duplicateTrack(const TrackId& trackId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.duplicateTrack(trackId);
}

bool AudioEngine::moveTrack(const TrackId& trackId, int direction)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.moveTrack(trackId, direction);
}

bool AudioEngine::removeTrack(const TrackId& trackId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.removeTrack(trackId);
}

void AudioEngine::newProject()
{
    stop();

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    projectModel.clearProject();
    projectModel.addAudioTrack();
    projectModel.addMidiTrack();
    currentFile = juce::File();
    lastRecordingFile = juce::File();
    activeRecordingSequence.clear();
    recordingMidi.store(false);
    loopEnabled.store(false);
    clearLoopRange();
}

bool AudioEngine::loadFile(const juce::File& file)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (projectModel.getAudioTracks().empty())
        projectModel.addAudioTrack();

    return loadAudioFileIntoTrack(*projectModel.getAudioTracks().front(), file);
}

bool AudioEngine::loadAudioFileToTrack(const TrackId& trackId, const juce::File& file)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        return loadAudioFileIntoTrack(*track, file);

    return false;
}

void AudioEngine::play()
{
    if (getLength() <= 0.0)
        return;

    metronome.reset(getPosition());
    transportState.store(TransportState::Playing);
}

void AudioEngine::pause()
{
    if (isRecording())
        stopRecording();

    transportState.store(TransportState::Paused);
    panic();
}

void AudioEngine::stop()
{
    if (isRecording())
        stopRecording();

    transportState.store(TransportState::Stopped);
    transportSamplePosition.store(0);
    metronome.reset(0.0);
    panic();
}

void AudioEngine::panic()
{
    std::scoped_lock lock(modelMutex);
    midiBuffer.clear();
    synth.allNotesOff();

    if (keyboardState != nullptr)
        for (auto channel = 1; channel <= 16; ++channel)
            for (auto note = 0; note < 128; ++note)
                keyboardState->noteOff(channel, note, 0.0f);
}

bool AudioEngine::startRecording()
{
    std::scoped_lock lock(modelMutex);
    const auto sampleRate = currentSampleRate.load();

    if (getFirstArmedAudioTrack() != nullptr)
    {
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("AI-DAW Recordings");
        dir.createDirectory();
        const auto file = dir.getChildFile("recording_"
                                           + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")
                                           + ".wav");

        if (! recorder.startRecording(file, sampleRate, 2))
            return false;

        lastRecordingFile = file;
    }

    if (getFirstArmedMidiTrack() != nullptr)
    {
        activeRecordingSequence.clear();
        recordingStartBeats = projectModel.getTempoMap().secondsToBeats(getPosition());
        recordingMidi.store(true);
    }

    transportState.store(TransportState::Recording);
    metronome.reset(getPosition());
    return recorder.isRecording() || recordingMidi.load();
}

void AudioEngine::stopRecording()
{
    recorder.stopRecording();

    std::scoped_lock lock(modelMutex);

    if (auto* track = getFirstArmedAudioTrack(); track != nullptr && lastRecordingFile.existsAsFile())
    {
        saveUndoSnapshotNoLock();
        loadAudioFileIntoTrack(*track, lastRecordingFile);
        lastRecordingFile = juce::File();
    }

    if (recordingMidi.exchange(false))
    {
        if (auto* track = getFirstArmedMidiTrack())
        {
            saveUndoSnapshotNoLock();
            activeRecordingSequence.updateMatchedPairs();
            MidiClip clip;
            clip.startBeat = recordingStartBeats;
            clip.lengthBeats = juce::jmax(1.0, projectModel.getTempoMap().secondsToBeats(getPosition())
                                                - recordingStartBeats);
            clip.sequence = activeRecordingSequence;
            track->clips.push_back(clip);
        }

        activeRecordingSequence.clear();
    }
}

void AudioEngine::setPosition(double seconds)
{
    if (! std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    const auto sampleRate = currentSampleRate.load();
    transportSamplePosition.store(static_cast<int64_t>(seconds * sampleRate));
    metronome.reset(seconds);
}

double AudioEngine::getPosition() const
{
    const auto sampleRate = currentSampleRate.load();
    return sampleRate > 0.0 ? static_cast<double>(transportSamplePosition.load()) / sampleRate : 0.0;
}

double AudioEngine::getLength() const
{
    std::scoped_lock lock(modelMutex);
    return projectModel.getProjectLengthSeconds();
}

void AudioEngine::setBpm(double bpm)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    projectModel.setBpm(bpm);
    metronome.setBpm(projectModel.getBpm());
}

double AudioEngine::getBpm() const
{
    std::scoped_lock lock(modelMutex);
    return projectModel.getBpm();
}

void AudioEngine::setGain(float gain)
{
    masterGain.store(juce::jlimit(0.0f, 1.0f, gain));
}

float AudioEngine::getGain() const
{
    return masterGain.load();
}

void AudioEngine::setMetronomeEnabled(bool enabled)
{
    metronome.setEnabled(enabled);
}

bool AudioEngine::isMetronomeEnabled() const
{
    return metronome.isEnabled();
}

void AudioEngine::setLoopEnabled(bool enabled)
{
    loopEnabled.store(enabled);
}

void AudioEngine::setLoopRange(double startSeconds, double endSeconds)
{
    if (! std::isfinite(startSeconds) || startSeconds < 0.0)
        startSeconds = 0.0;

    if (! std::isfinite(endSeconds) || endSeconds <= startSeconds)
    {
        clearLoopRange();
        return;
    }

    loopStartSeconds.store(startSeconds);
    loopEndSeconds.store(endSeconds);
    loopEnabled.store(true);
}

void AudioEngine::clearLoopRange()
{
    loopStartSeconds.store(0.0);
    loopEndSeconds.store(0.0);
}

bool AudioEngine::isLoopEnabled() const
{
    return loopEnabled.load();
}

bool AudioEngine::isPlaying() const
{
    const auto state = transportState.load();
    return state == TransportState::Playing || state == TransportState::Recording;
}

bool AudioEngine::isRecording() const
{
    return transportState.load() == TransportState::Recording;
}

bool AudioEngine::hasLoadedFile() const
{
    std::scoped_lock lock(modelMutex);

    for (const auto& track : projectModel.getAudioTracks())
        if (track->hasAudio())
            return true;

    for (const auto& track : projectModel.getMidiTracks())
        if (! track->clips.empty())
            return true;

    return false;
}

const juce::File& AudioEngine::getCurrentFile() const
{
    return currentFile;
}

void AudioEngine::setTrackGain(const TrackId& trackId, float gain)
{
    std::scoped_lock lock(modelMutex);

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.gain = juce::jlimit(0.0f, 1.0f, gain);
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.gain = juce::jlimit(0.0f, 1.0f, gain);
}

void AudioEngine::setTrackPan(const TrackId& trackId, float pan)
{
    std::scoped_lock lock(modelMutex);

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.pan = juce::jlimit(-1.0f, 1.0f, pan);
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.pan = juce::jlimit(-1.0f, 1.0f, pan);
}

bool AudioEngine::renameTrack(const TrackId& trackId, const juce::String& newName)
{
    auto cleanName = newName.trim();

    if (cleanName.isEmpty())
        return false;

    cleanName = cleanName.substring(0, 48);

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
    {
        track->state.name = cleanName;
        return true;
    }

    if (auto* track = projectModel.findMidiTrack(trackId))
    {
        track->state.name = cleanName;
        return true;
    }

    return false;
}

void AudioEngine::setTrackMuted(const TrackId& trackId, bool muted)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.muted = muted;
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.muted = muted;
}

void AudioEngine::setTrackSolo(const TrackId& trackId, bool solo)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.solo = solo;
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.solo = solo;
}

void AudioEngine::setTrackArmed(const TrackId& trackId, bool armed)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    for (auto& track : projectModel.getAudioTracks())
        track->state.armed = false;

    for (auto& track : projectModel.getMidiTracks())
        track->state.armed = false;

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.armed = armed;
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.armed = armed;
}

void AudioEngine::setAudioClipStartTime(const TrackId& trackId,
                                        const juce::Uuid& clipId,
                                        double startTimeSeconds)
{
    if (! std::isfinite(startTimeSeconds) || startTimeSeconds < 0.0)
        startTimeSeconds = 0.0;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.startTimeSeconds = startTimeSeconds;
                return;
            }
}

void AudioEngine::moveAudioClipToTrack(const TrackId& sourceTrackId,
                                       const TrackId& destinationTrackId,
                                       const juce::Uuid& clipId,
                                       double startTimeSeconds)
{
    if (! std::isfinite(startTimeSeconds) || startTimeSeconds < 0.0)
        startTimeSeconds = 0.0;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    auto* sourceTrack = projectModel.findAudioTrack(sourceTrackId);
    auto* destinationTrack = projectModel.findAudioTrack(destinationTrackId);

    if (sourceTrack == nullptr || destinationTrack == nullptr || ! sourceTrack->hasAudio())
        return;

    auto clipIterator = std::find_if(sourceTrack->clips.begin(), sourceTrack->clips.end(),
                                     [&clipId](const AudioClip& clip)
                                     {
                                         return clip.id == clipId;
                                     });

    if (clipIterator == sourceTrack->clips.end())
        return;

    if (sourceTrack == destinationTrack)
    {
        clipIterator->startTimeSeconds = startTimeSeconds;
        return;
    }

    destinationTrack->audioBuffer = sourceTrack->audioBuffer;
    destinationTrack->sampleRate = sourceTrack->sampleRate;
    auto movedClip = *clipIterator;
    movedClip.startTimeSeconds = startTimeSeconds;
    destinationTrack->clips.push_back(movedClip);
    sourceTrack->clips.erase(clipIterator);

    if (sourceTrack->clips.empty())
    {
        sourceTrack->audioBuffer.setSize(0, 0);
        sourceTrack->sampleRate = 0.0;
    }
}

bool AudioEngine::setAudioClipTiming(const TrackId& trackId,
                                     const juce::Uuid& clipId,
                                     double startTimeSeconds,
                                     double sourceOffsetSeconds,
                                     double lengthSeconds)
{
    if (! std::isfinite(startTimeSeconds)
        || ! std::isfinite(sourceOffsetSeconds)
        || ! std::isfinite(lengthSeconds))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
    {
        const auto sourceDuration = track->getAudioDurationSeconds();

        for (auto& clip : track->clips)
        {
            if (clip.id != clipId)
                continue;

            const auto safeOffset = juce::jlimit(0.0, sourceDuration, sourceOffsetSeconds);
            const auto maxLength = juce::jmax(0.0, sourceDuration - safeOffset);

            if (maxLength < 0.05)
                return false;

            const auto safeLength = juce::jlimit(0.05, maxLength, lengthSeconds);

            if (safeLength <= 0.0)
                return false;

            clip.startTimeSeconds = juce::jmax(0.0, startTimeSeconds);
            clip.sourceOffsetSeconds = safeOffset;
            clip.lengthSeconds = safeLength;
            clip.fadeInSeconds = juce::jlimit(0.0, clip.lengthSeconds * 0.5, clip.fadeInSeconds);
            clip.fadeOutSeconds = juce::jlimit(0.0, clip.lengthSeconds * 0.5, clip.fadeOutSeconds);
            return true;
        }
    }

    return false;
}

bool AudioEngine::duplicateAudioClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto duplicate = clip;
                duplicate.id = juce::Uuid();
                duplicate.startTimeSeconds += juce::jmax(1.0, clip.lengthSeconds * 0.25);
                track->clips.push_back(duplicate);
                return true;
            }

    return false;
}

bool AudioEngine::deleteAudioClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
    {
        const auto oldSize = track->clips.size();
        track->clips.erase(std::remove_if(track->clips.begin(),
                                          track->clips.end(),
                                          [&clipId](const AudioClip& clip)
                                          {
                                              return clip.id == clipId;
                                          }),
                           track->clips.end());

        if (track->clips.empty())
        {
            track->audioBuffer.setSize(0, 0);
            track->sampleRate = 0.0;
        }

        return track->clips.size() != oldSize;
    }

    return false;
}

bool AudioEngine::splitAudioClipAtPosition(const TrackId& trackId,
                                           const juce::Uuid& clipId,
                                           double positionSeconds)
{
    if (! std::isfinite(positionSeconds))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
    {
        for (auto& clip : track->clips)
        {
            if (clip.id != clipId)
                continue;

            const auto localSplit = positionSeconds - clip.startTimeSeconds;

            if (localSplit <= 0.05 || localSplit >= clip.lengthSeconds - 0.05)
                return false;

            auto rightClip = clip;
            rightClip.id = juce::Uuid();
            rightClip.startTimeSeconds = positionSeconds;
            rightClip.sourceOffsetSeconds += localSplit;
            rightClip.lengthSeconds -= localSplit;
            rightClip.fadeInSeconds = 0.0;

            clip.lengthSeconds = localSplit;
            clip.fadeOutSeconds = 0.0;
            track->clips.push_back(rightClip);
            return true;
        }
    }

    return false;
}

bool AudioEngine::setAudioClipFade(const TrackId& trackId,
                                   const juce::Uuid& clipId,
                                   double fadeInSeconds,
                                   double fadeOutSeconds)
{
    if (! std::isfinite(fadeInSeconds) || fadeInSeconds < 0.0)
        fadeInSeconds = 0.0;

    if (! std::isfinite(fadeOutSeconds) || fadeOutSeconds < 0.0)
        fadeOutSeconds = 0.0;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                const auto maxFade = clip.lengthSeconds * 0.5;
                clip.fadeInSeconds = juce::jlimit(0.0, maxFade, fadeInSeconds);
                clip.fadeOutSeconds = juce::jlimit(0.0, maxFade, fadeOutSeconds);
                return true;
            }

    return false;
}

bool AudioEngine::adjustAudioClipGain(const TrackId& trackId, const juce::Uuid& clipId, float delta)
{
    if (! std::isfinite(delta) || delta == 0.0f)
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.gain = juce::jlimit(0.0f, 2.0f, clip.gain + delta);
                return true;
            }

    return false;
}

void AudioEngine::setMidiClipStartBeat(const TrackId& trackId,
                                       const juce::Uuid& clipId,
                                       double startBeat)
{
    if (! std::isfinite(startBeat) || startBeat < 0.0)
        startBeat = 0.0;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.startBeat = startBeat;
                return;
            }
}

bool AudioEngine::setMidiClipLength(const TrackId& trackId,
                                    const juce::Uuid& clipId,
                                    double lengthBeats)
{
    if (! std::isfinite(lengthBeats))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.lengthBeats = juce::jmax(0.25, lengthBeats);
                return true;
            }

    return false;
}

void AudioEngine::moveMidiClipToTrack(const TrackId& sourceTrackId,
                                      const TrackId& destinationTrackId,
                                      const juce::Uuid& clipId,
                                      double startBeat)
{
    if (! std::isfinite(startBeat) || startBeat < 0.0)
        startBeat = 0.0;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    auto* sourceTrack = projectModel.findMidiTrack(sourceTrackId);
    auto* destinationTrack = projectModel.findMidiTrack(destinationTrackId);

    if (sourceTrack == nullptr || destinationTrack == nullptr)
        return;

    auto clipIterator = std::find_if(sourceTrack->clips.begin(),
                                     sourceTrack->clips.end(),
                                     [&clipId](const MidiClip& clip)
                                     {
                                         return clip.id == clipId;
                                     });

    if (clipIterator == sourceTrack->clips.end())
        return;

    if (sourceTrack == destinationTrack)
    {
        clipIterator->startBeat = startBeat;
        return;
    }

    auto movedClip = *clipIterator;
    movedClip.startBeat = startBeat;
    destinationTrack->clips.push_back(movedClip);
    sourceTrack->clips.erase(clipIterator);
}

bool AudioEngine::duplicateMidiClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto duplicate = clip;
                duplicate.id = juce::Uuid();
                duplicate.startBeat += juce::jmax(1.0, clip.lengthBeats);
                track->clips.push_back(duplicate);
                return true;
            }

    return false;
}

bool AudioEngine::deleteMidiClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
    {
        const auto oldSize = track->clips.size();
        track->clips.erase(std::remove_if(track->clips.begin(),
                                          track->clips.end(),
                                          [&clipId](const MidiClip& clip)
                                          {
                                              return clip.id == clipId;
                                          }),
                           track->clips.end());

        return track->clips.size() != oldSize;
    }

    return false;
}

bool AudioEngine::quantizeMidiClip(const TrackId& trackId, const juce::Uuid& clipId, double gridBeats)
{
    if (! std::isfinite(gridBeats) || gridBeats <= 0.0)
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr)
                        continue;

                    auto message = event->message;
                    auto timeStamp = message.getTimeStamp();

                    if (! std::isfinite(timeStamp))
                        timeStamp = 0.0;

                    const auto quantized = juce::jlimit(0.0,
                                                        clip.lengthBeats,
                                                        std::round(timeStamp / gridBeats) * gridBeats);
                    message.setTimeStamp(quantized);
                    event->message = message;
                }

                clip.sequence.sort();
                clip.sequence.updateMatchedPairs();
                return true;
            }

    return false;
}

bool AudioEngine::transposeMidiClip(const TrackId& trackId, const juce::Uuid& clipId, int semitones)
{
    if (semitones == 0)
        return true;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr)
                        continue;

                    auto message = event->message;

                    if (! message.isNoteOnOrOff())
                        continue;

                    message.setNoteNumber(juce::jlimit(0, 127, message.getNoteNumber() + semitones));
                    event->message = message;
                }

                clip.sequence.updateMatchedPairs();
                return true;
            }

    return false;
}

bool AudioEngine::adjustMidiClipVelocity(const TrackId& trackId, const juce::Uuid& clipId, float delta)
{
    if (! std::isfinite(delta) || delta == 0.0f)
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr)
                        continue;

                    auto message = event->message;

                    if (! message.isNoteOn())
                        continue;

                    const auto velocity = juce::jlimit(0.0f, 1.0f, message.getFloatVelocity() + delta);
                    message.setVelocity(velocity);
                    event->message = message;
                }

                clip.sequence.updateMatchedPairs();
                return true;
            }

    return false;
}

bool AudioEngine::generateChordProgression(const TrackId& trackId, const juce::String& style)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
    {
        track->clips.push_back(createChordProgressionClip(style));
        return true;
    }

    return false;
}

void AudioEngine::setMidiKeyboardState(juce::MidiKeyboardState* state)
{
    keyboardState = state;
}

bool AudioEngine::saveProject(const juce::File& file)
{
    std::scoped_lock lock(modelMutex);
    const auto projectDirectory = file.getParentDirectory();
    const auto audioDirectory = projectDirectory.getChildFile("Audio");

    if (! projectDirectory.createDirectory() || ! audioDirectory.createDirectory())
        return false;

    std::map<juce::String, juce::File> copiedFiles;

    for (auto& track : projectModel.getAudioTracks())
    {
        for (auto& clip : track->clips)
        {
            if (! clip.sourceFile.existsAsFile())
                return false;

            if (! clip.sourceFile.isAChildOf(audioDirectory))
            {
                const auto sourcePath = clip.sourceFile.getFullPathName();

                if (const auto existing = copiedFiles.find(sourcePath); existing != copiedFiles.end())
                {
                    clip.sourceFile = existing->second;
                    continue;
                }

                const auto destination = getAvailableCopyDestination(audioDirectory, clip.sourceFile);

                if (destination != clip.sourceFile
                    && ! clip.sourceFile.copyFileTo(destination))
                    return false;

                copiedFiles[sourcePath] = destination;
                clip.sourceFile = destination;
            }
        }
    }

    return file.replaceWithText(projectModel.toJsonString(projectDirectory));
}

bool AudioEngine::loadProject(const juce::File& file)
{
    std::scoped_lock lock(modelMutex);

    if (! projectModel.loadFromFile(file))
        return false;

    for (auto& track : projectModel.getAudioTracks())
        if (! track->clips.empty())
            loadAudioBufferForTrack(*track, track->clips.front().sourceFile);

    undoStack.clear();
    redoStack.clear();
    return true;
}

bool AudioEngine::exportToWav(const juce::File& destinationFile)
{
    auto destination = destinationFile.hasFileExtension(".wav")
        ? destinationFile
        : destinationFile.withFileExtension(".wav");

    if (destination.existsAsFile() && ! destination.deleteFile())
        return false;

    std::unique_ptr<juce::OutputStream> outputStream = destination.createOutputStream();

    if (outputStream == nullptr)
        return false;

    constexpr double outputSampleRate = 44100.0;
    constexpr int outputChannels = 2;
    constexpr int bitsPerSample = 16;
    constexpr int blockSize = 8192;
    const auto lengthSeconds = getLength();
    const auto totalSamples = static_cast<int>(std::ceil(lengthSeconds * outputSampleRate));

    if (totalSamples <= 0)
        return false;

    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate(outputSampleRate)
                             .withNumChannels(outputChannels)
                             .withBitsPerSample(bitsPerSample);
    auto writer = wavFormat.createWriterFor(outputStream, writerOptions);

    if (writer == nullptr)
        return false;

    SimpleSynth exportSynth;
    exportSynth.setCurrentPlaybackSampleRate(outputSampleRate);
    juce::AudioBuffer<float> buffer(outputChannels, blockSize);
    juce::MidiBuffer exportMidi;

    for (auto rendered = 0; rendered < totalSamples;)
    {
        const auto numToRender = juce::jmin(blockSize, totalSamples - rendered);
        buffer.setSize(outputChannels, numToRender, false, false, true);
        buffer.clear();
        exportMidi.clear();

        {
            std::scoped_lock lock(modelMutex);
            renderAudioTracks(buffer, numToRender, static_cast<double>(rendered) / outputSampleRate);
            renderMidiTracks(buffer,
                             numToRender,
                             static_cast<double>(rendered) / outputSampleRate,
                             exportMidi,
                             exportSynth);
        }

        buffer.applyGain(masterGain.load());

        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
                buffer.setSample(channel, sample, juce::jlimit(-1.0f, 1.0f, buffer.getSample(channel, sample)));

        if (! writer->writeFromAudioSampleBuffer(buffer, 0, numToRender))
            return false;

        rendered += numToRender;
    }

    writer->flush();
    return true;
}

bool AudioEngine::undo()
{
    std::scoped_lock lock(modelMutex);

    if (undoStack.empty())
        return false;

    redoStack.push_back(projectModel.toJsonString());
    const auto snapshot = undoStack.back();
    undoStack.pop_back();
    return restoreProjectSnapshotNoLock(snapshot);
}

bool AudioEngine::redo()
{
    std::scoped_lock lock(modelMutex);

    if (redoStack.empty())
        return false;

    undoStack.push_back(projectModel.toJsonString());
    const auto snapshot = redoStack.back();
    redoStack.pop_back();
    return restoreProjectSnapshotNoLock(snapshot);
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    if (numOutputChannels <= 0 || numSamples <= 0)
        return;

    if (recorder.isRecording())
        recorder.processInput(inputChannelData, numInputChannels, numSamples);

    if (numSamples > renderBuffer.getNumSamples() || numOutputChannels > renderBuffer.getNumChannels())
    {
        for (auto channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);

        return;
    }

    renderBuffer.setSize(numOutputChannels, numSamples, false, false, true);
    renderBuffer.clear();

    const auto blockStartSeconds = getPosition();

    {
        std::scoped_lock lock(modelMutex);
        renderToBuffer(renderBuffer, numSamples, blockStartSeconds, true);
    }

    renderBuffer.applyGain(masterGain.load());

    for (auto channel = 0; channel < numOutputChannels; ++channel)
    {
        auto* output = outputChannelData[channel];

        if (output == nullptr)
            continue;

        const auto sourceChannel = juce::jmin(channel, renderBuffer.getNumChannels() - 1);
        juce::FloatVectorOperations::copy(output, renderBuffer.getReadPointer(sourceChannel), numSamples);

        for (auto sample = 0; sample < numSamples; ++sample)
            output[sample] = juce::jlimit(-1.0f, 1.0f, output[sample]);
    }

    if (isPlaying())
    {
        const auto nextPosition = transportSamplePosition.load() + numSamples;
        transportSamplePosition.store(nextPosition);

        const auto length = getLength();
        const auto loopStart = loopStartSeconds.load();
        const auto loopEnd = loopEndSeconds.load();
        const auto hasCustomLoopRange = loopEnd > loopStart;
        const auto endPosition = hasCustomLoopRange ? loopEnd : length;

        if (endPosition > 0.0 && getPosition() >= endPosition)
        {
            if (loopEnabled.load())
                setPosition(hasCustomLoopRange ? loopStart : 0.0);
            else
                stop();
        }
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    const auto blockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
    currentSampleRate.store(sampleRate);
    renderBuffer.setSize(2, blockSize * 4);
    synth.setCurrentPlaybackSampleRate(sampleRate);
    metronome.prepare(sampleRate);
    metronome.setBpm(getBpm());
}

void AudioEngine::audioDeviceStopped()
{
    recorder.stopRecording();
}

bool AudioEngine::loadAudioFileIntoTrack(AudioTrack& track, const juce::File& file)
{
    if (! loadAudioBufferForTrack(track, file))
        return false;

    track.clips.clear();
    AudioClip clip;
    clip.id = juce::Uuid();
    clip.sourceFile = file;
    clip.lengthSeconds = track.getAudioDurationSeconds();
    clip.startTimeSeconds = getPosition();
    track.clips.push_back(clip);
    currentFile = file;
    return true;
}

bool AudioEngine::loadAudioBufferForTrack(AudioTrack& track, const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return false;

    const auto numChannels = juce::jmax(1, static_cast<int>(reader->numChannels));
    const auto numSamples = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> newBuffer(numChannels, numSamples);
    reader->read(&newBuffer, 0, numSamples, 0, true, true);

    track.audioBuffer = std::move(newBuffer);
    track.sampleRate = reader->sampleRate;
    currentFile = file;
    return true;
}

void AudioEngine::renderToBuffer(juce::AudioBuffer<float>& buffer,
                                 int numSamples,
                                 double blockStartSeconds,
                                 bool includeMetronome)
{
    midiBuffer.clear();
    renderAudioTracks(buffer, numSamples, blockStartSeconds);

    if (keyboardState != nullptr)
        keyboardState->processNextMidiBuffer(midiBuffer, 0, numSamples, true);

    if (recordingMidi.load())
    {
        for (const auto metadata : midiBuffer)
        {
            auto message = metadata.getMessage();
            const auto eventSeconds = blockStartSeconds
                                    + static_cast<double>(metadata.samplePosition)
                                      / currentSampleRate.load();
            message.setTimeStamp(projectModel.getTempoMap().secondsToBeats(eventSeconds)
                                 - recordingStartBeats);
            activeRecordingSequence.addEvent(message);
        }
    }

    renderMidiTracks(buffer, numSamples, blockStartSeconds, midiBuffer, synth);

    if (includeMetronome)
        metronome.renderNextBlock(buffer, 0, numSamples, blockStartSeconds);
}

void AudioEngine::renderAudioTracks(juce::AudioBuffer<float>& buffer,
                                    int numSamples,
                                    double blockStartSeconds) const
{
    const auto anySolo = anySoloedTrack();
    const auto outputSampleRate = currentSampleRate.load();

    for (const auto& trackPtr : projectModel.getAudioTracks())
    {
        const auto& track = *trackPtr;

        if (! track.hasAudio() || ! shouldRenderTrack(track.state, anySolo))
            continue;

        if (track.clips.empty())
            continue;

        for (const auto& clip : track.clips)
        {
            for (auto sample = 0; sample < numSamples; ++sample)
            {
                const auto timeSeconds = blockStartSeconds + static_cast<double>(sample) / outputSampleRate;
                const auto clipTime = timeSeconds - clip.startTimeSeconds;
                const auto sourceTime = clipTime + clip.sourceOffsetSeconds;

                if (clipTime < 0.0 || clipTime >= clip.lengthSeconds)
                    continue;

                const auto sourceIndex = static_cast<int>(sourceTime * track.sampleRate);

                if (sourceIndex < 0 || sourceIndex >= track.audioBuffer.getNumSamples())
                    continue;

                auto clipGain = track.state.gain * clip.gain;

                if (clip.fadeInSeconds > 0.0 && clipTime < clip.fadeInSeconds)
                    clipGain *= static_cast<float>(clipTime / clip.fadeInSeconds);

                if (clip.fadeOutSeconds > 0.0 && clipTime > clip.lengthSeconds - clip.fadeOutSeconds)
                    clipGain *= static_cast<float>((clip.lengthSeconds - clipTime) / clip.fadeOutSeconds);

                clipGain = juce::jlimit(0.0f, 1.0f, clipGain);
                const auto pan = juce::jlimit(-1.0f, 1.0f, track.state.pan);
                const auto leftPanGain = pan > 0.0f ? 1.0f - pan : 1.0f;
                const auto rightPanGain = pan < 0.0f ? 1.0f + pan : 1.0f;

                for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    const auto sourceChannel = juce::jmin(channel, track.audioBuffer.getNumChannels() - 1);
                    const auto panGain = channel == 0 ? leftPanGain : (channel == 1 ? rightPanGain : 1.0f);
                    buffer.addSample(channel,
                                     sample,
                                     track.audioBuffer.getSample(sourceChannel, sourceIndex)
                                         * clipGain
                                         * panGain);
                }
            }
        }
    }
}

void AudioEngine::renderMidiTracks(juce::AudioBuffer<float>& buffer,
                                   int numSamples,
                                   double blockStartSeconds,
                                   juce::MidiBuffer& midiMessages,
                                   SimpleSynth& synthToUse) const
{
    const auto anySolo = anySoloedTrack();
    const auto sampleRate = currentSampleRate.load();
    const auto& tempo = projectModel.getTempoMap();

    for (const auto& trackPtr : projectModel.getMidiTracks())
    {
        const auto& track = *trackPtr;

        if (! shouldRenderTrack(track.state, anySolo))
            continue;

        for (const auto& clip : track.clips)
        {
            for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
            {
                const auto* event = clip.sequence.getEventPointer(i);

                if (event == nullptr)
                    continue;

                const auto eventBeat = event->message.getTimeStamp();

                if (eventBeat < 0.0 || eventBeat >= clip.lengthBeats)
                    continue;

                const auto eventSeconds = tempo.beatsToSeconds(clip.startBeat
                                             + eventBeat);
                const auto offset = static_cast<int>((eventSeconds - blockStartSeconds) * sampleRate);

                if (offset >= 0 && offset < numSamples)
                {
                    auto message = event->message;

                    if (message.isNoteOn())
                        message.setVelocity(juce::jlimit(0.0f,
                                                         1.0f,
                                                         message.getFloatVelocity() * track.state.gain));

                    midiMessages.addEvent(message, offset);
                }
            }
        }
    }

    synthToUse.renderNextBlock(buffer, midiMessages, 0, numSamples);
}

bool AudioEngine::anySoloedTrack() const
{
    for (const auto& track : projectModel.getAudioTracks())
        if (track->state.solo)
            return true;

    for (const auto& track : projectModel.getMidiTracks())
        if (track->state.solo)
            return true;

    return false;
}

bool AudioEngine::shouldRenderTrack(const TrackState& state, bool anySolo) const
{
    return ! state.muted && (! anySolo || state.solo);
}

MidiClip AudioEngine::createChordProgressionClip(const juce::String& style) const
{
    const auto normalized = style.toLowerCase();
    const std::array<std::array<int, 3>, 4> pop {
        std::array<int, 3> { 60, 64, 67 },
        std::array<int, 3> { 67, 71, 74 },
        std::array<int, 3> { 69, 72, 76 },
        std::array<int, 3> { 65, 69, 72 }
    };
    const std::array<std::array<int, 3>, 4> minor {
        std::array<int, 3> { 57, 60, 64 },
        std::array<int, 3> { 65, 69, 72 },
        std::array<int, 3> { 52, 55, 59 },
        std::array<int, 3> { 64, 67, 71 }
    };
    const auto& chords = normalized.contains("minor") ? minor : pop;

    MidiClip clip;
    clip.id = juce::Uuid();
    clip.startBeat = projectModel.getTempoMap().secondsToBeats(getPosition());
    clip.lengthBeats = 16.0;

    for (auto chordIndex = 0; chordIndex < static_cast<int>(chords.size()); ++chordIndex)
    {
        const auto beat = static_cast<double>(chordIndex) * 4.0;

        for (const auto note : chords[static_cast<size_t>(chordIndex)])
        {
            auto noteOn = juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(92));
            noteOn.setTimeStamp(beat);
            auto noteOff = juce::MidiMessage::noteOff(1, note);
            noteOff.setTimeStamp(beat + 3.8);
            clip.sequence.addEvent(noteOn);
            clip.sequence.addEvent(noteOff);
        }
    }

    clip.sequence.updateMatchedPairs();
    return clip;
}

AudioTrack* AudioEngine::getFirstArmedAudioTrack()
{
    for (auto& track : projectModel.getAudioTracks())
        if (track->state.armed)
            return track.get();

    return nullptr;
}

MidiTrack* AudioEngine::getFirstArmedMidiTrack()
{
    for (auto& track : projectModel.getMidiTracks())
        if (track->state.armed)
            return track.get();

    return nullptr;
}

void AudioEngine::saveUndoSnapshotNoLock()
{
    undoStack.push_back(projectModel.toJsonString());

    constexpr auto maxUndoSnapshots = 100;
    if (undoStack.size() > maxUndoSnapshots)
        undoStack.erase(undoStack.begin());

    redoStack.clear();
}

bool AudioEngine::restoreProjectSnapshotNoLock(const juce::String& snapshot)
{
    if (! projectModel.loadFromJsonString(snapshot))
        return false;

    for (auto& track : projectModel.getAudioTracks())
        if (! track->clips.empty())
            loadAudioBufferForTrack(*track, track->clips.front().sourceFile);

    const auto length = projectModel.getProjectLengthSeconds();

    if (getPosition() > length)
        setPosition(length);

    return true;
}
