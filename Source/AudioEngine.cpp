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

juce::Uuid AudioEngine::addMarker(double timeSeconds)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.addMarker(timeSeconds);
}

bool AudioEngine::renameMarker(const juce::Uuid& markerId, const juce::String& name)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.renameMarker(markerId, name);
}

bool AudioEngine::removeNearestMarker(double timeSeconds, double thresholdSeconds)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();
    return projectModel.removeNearestMarker(timeSeconds, thresholdSeconds);
}

void AudioEngine::setGain(float gain)
{
    masterGain.store(juce::jlimit(0.0f, 1.0f, gain));
}

float AudioEngine::getGain() const
{
    return masterGain.load();
}

float AudioEngine::getLastOutputPeak() const
{
    return lastOutputPeak.load();
}

float AudioEngine::getHeldOutputPeak() const
{
    return heldOutputPeak.load();
}

void AudioEngine::resetHeldOutputPeak()
{
    heldOutputPeak.store(0.0f);
}

void AudioEngine::setMetronomeEnabled(bool enabled)
{
    metronome.setEnabled(enabled);
}

bool AudioEngine::isMetronomeEnabled() const
{
    return metronome.isEnabled();
}

void AudioEngine::setMetronomeGain(float gain)
{
    metronome.setGain(gain);
}

float AudioEngine::getMetronomeGain() const
{
    return metronome.getGain();
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

void AudioEngine::soloOnlyTrack(const TrackId& trackId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    for (auto& track : projectModel.getAudioTracks())
        track->state.solo = track->state.id == trackId;

    for (auto& track : projectModel.getMidiTracks())
        track->state.solo = track->state.id == trackId;
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

void AudioEngine::armOnlyTrack(const TrackId& trackId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    for (auto& track : projectModel.getAudioTracks())
        track->state.armed = track->state.id == trackId;

    for (auto& track : projectModel.getMidiTracks())
        track->state.armed = track->state.id == trackId;
}

void AudioEngine::clearTrackMuteSolo()
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    for (auto& track : projectModel.getAudioTracks())
    {
        track->state.muted = false;
        track->state.solo = false;
    }

    for (auto& track : projectModel.getMidiTracks())
    {
        track->state.muted = false;
        track->state.solo = false;
    }
}

void AudioEngine::clearTrackArms()
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    for (auto& track : projectModel.getAudioTracks())
        track->state.armed = false;

    for (auto& track : projectModel.getMidiTracks())
        track->state.armed = false;
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

bool AudioEngine::duplicateAudioClipAtTime(const TrackId& trackId,
                                           const juce::Uuid& clipId,
                                           double startTimeSeconds)
{
    if (! std::isfinite(startTimeSeconds))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto duplicate = clip;
                duplicate.id = juce::Uuid();
                duplicate.startTimeSeconds = juce::jmax(0.0, startTimeSeconds);
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

bool AudioEngine::clearAudioClipFades(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.fadeInSeconds = 0.0;
                clip.fadeOutSeconds = 0.0;
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

bool AudioEngine::setAudioClipGain(const TrackId& trackId, const juce::Uuid& clipId, float gain)
{
    if (! std::isfinite(gain))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.gain = juce::jlimit(0.0f, 2.0f, gain);
                return true;
            }

    return false;
}

bool AudioEngine::normalizeAudioClipGain(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                if (! track->hasAudio() || track->sampleRate <= 0.0)
                    return false;

                const auto startSample = juce::jlimit(0,
                                                      track->audioBuffer.getNumSamples(),
                                                      static_cast<int>(clip.sourceOffsetSeconds * track->sampleRate));
                const auto endSample = juce::jlimit(startSample,
                                                    track->audioBuffer.getNumSamples(),
                                                    static_cast<int>((clip.sourceOffsetSeconds + clip.lengthSeconds)
                                                        * track->sampleRate));
                const auto numSamples = endSample - startSample;

                if (numSamples <= 0)
                    return false;

                auto peak = 0.0f;

                for (auto channel = 0; channel < track->audioBuffer.getNumChannels(); ++channel)
                    peak = juce::jmax(peak,
                                      track->audioBuffer.getMagnitude(channel, startSample, numSamples));

                if (peak <= 0.0001f)
                    return false;

                clip.gain = juce::jlimit(0.0f, 2.0f, 0.95f / peak);
                return true;
            }

    return false;
}

bool AudioEngine::toggleAudioClipMuted(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.muted = ! clip.muted;
                return true;
            }

    return false;
}

bool AudioEngine::toggleAudioClipReversed(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findAudioTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.reversed = ! clip.reversed;
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

bool AudioEngine::duplicateMidiClipAtBeat(const TrackId& trackId,
                                          const juce::Uuid& clipId,
                                          double startBeat)
{
    if (! std::isfinite(startBeat))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto duplicate = clip;
                duplicate.id = juce::Uuid();
                duplicate.startBeat = juce::jmax(0.0, startBeat);
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

bool AudioEngine::splitMidiClipAtBeat(const TrackId& trackId,
                                      const juce::Uuid& clipId,
                                      double splitBeat)
{
    if (! std::isfinite(splitBeat))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
    {
        for (auto& clip : track->clips)
        {
            if (clip.id != clipId)
                continue;

            const auto localSplit = splitBeat - clip.startBeat;

            if (localSplit <= 0.05 || localSplit >= clip.lengthBeats - 0.05)
                return false;

            clip.sequence.updateMatchedPairs();

            MidiClip rightClip;
            rightClip.startBeat = splitBeat;
            rightClip.lengthBeats = clip.lengthBeats - localSplit;
            rightClip.muted = clip.muted;

            juce::MidiMessageSequence leftSequence;
            juce::MidiMessageSequence rightSequence;

            const auto addMessage = [](juce::MidiMessageSequence& sequence,
                                       juce::MidiMessage message,
                                       double beat)
            {
                message.setTimeStamp(juce::jmax(0.0, beat));
                sequence.addEvent(message);
            };

            for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
            {
                const auto* event = clip.sequence.getEventPointer(i);

                if (event == nullptr)
                    continue;

                const auto message = event->message;
                const auto eventBeat = message.getTimeStamp();

                if (message.isNoteOn())
                {
                    const auto noteEndBeat = event->noteOffObject != nullptr
                        ? event->noteOffObject->message.getTimeStamp()
                        : clip.lengthBeats;

                    if (eventBeat < localSplit)
                    {
                        auto noteOn = message;
                        auto noteOff = juce::MidiMessage::noteOff(message.getChannel(),
                                                                  message.getNoteNumber(),
                                                                  static_cast<juce::uint8>(0));
                        addMessage(leftSequence, noteOn, eventBeat);
                        addMessage(leftSequence, noteOff, juce::jmin(noteEndBeat, localSplit));
                    }

                    if (noteEndBeat > localSplit)
                    {
                        auto noteOn = message;
                        auto noteOff = juce::MidiMessage::noteOff(message.getChannel(),
                                                                  message.getNoteNumber(),
                                                                  static_cast<juce::uint8>(0));
                        addMessage(rightSequence, noteOn, juce::jmax(eventBeat, localSplit) - localSplit);
                        addMessage(rightSequence, noteOff, noteEndBeat - localSplit);
                    }
                }
                else if (! message.isNoteOff())
                {
                    if (eventBeat < localSplit)
                        addMessage(leftSequence, message, eventBeat);
                    else if (eventBeat < clip.lengthBeats)
                        addMessage(rightSequence, message, eventBeat - localSplit);
                }
            }

            clip.lengthBeats = localSplit;
            clip.sequence = leftSequence;
            clip.sequence.sort();
            clip.sequence.updateMatchedPairs();

            rightClip.sequence = rightSequence;
            rightClip.sequence.sort();
            rightClip.sequence.updateMatchedPairs();

            track->clips.push_back(rightClip);
            return true;
        }
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

bool AudioEngine::swingQuantizeMidiClip(const TrackId& trackId,
                                        const juce::Uuid& clipId,
                                        double gridBeats,
                                        double swingAmount)
{
    if (! std::isfinite(gridBeats) || gridBeats <= 0.0 || ! std::isfinite(swingAmount))
        return false;

    const auto safeSwing = juce::jlimit(0.0, 0.75, swingAmount);
    const auto pairLength = gridBeats * 2.0;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.sequence.updateMatchedPairs();

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr)
                        continue;

                    auto message = event->message;
                    auto timeStamp = message.getTimeStamp();

                    if (! std::isfinite(timeStamp))
                        timeStamp = 0.0;

                    const auto pairIndex = std::floor(timeStamp / pairLength);
                    const auto pairStart = pairIndex * pairLength;
                    const auto local = timeStamp - pairStart;
                    const auto target = local < gridBeats * 0.5
                        ? pairStart
                        : pairStart + gridBeats + gridBeats * safeSwing;
                    message.setTimeStamp(juce::jlimit(0.0, clip.lengthBeats, target));
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

bool AudioEngine::invertMidiClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto noteSum = 0;
                auto noteCount = 0;

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    const auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr || ! event->message.isNoteOn())
                        continue;

                    noteSum += event->message.getNoteNumber();
                    ++noteCount;
                }

                if (noteCount == 0)
                    return false;

                const auto pivot = static_cast<double>(noteSum) / static_cast<double>(noteCount);

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr)
                        continue;

                    auto message = event->message;

                    if (! message.isNoteOnOrOff())
                        continue;

                    const auto invertedNote = static_cast<int>(std::round((2.0 * pivot)
                                                                          - static_cast<double>(message.getNoteNumber())));
                    message.setNoteNumber(juce::jlimit(0, 127, invertedNote));
                    event->message = message;
                }

                clip.sequence.updateMatchedPairs();
                return true;
            }

    return false;
}

bool AudioEngine::addMidiClipOctaveLayer(const TrackId& trackId, const juce::Uuid& clipId, int semitones)
{
    if (semitones == 0)
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                juce::MidiMessageSequence layer;

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    const auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr || ! event->message.isNoteOnOrOff())
                        continue;

                    auto message = event->message;
                    const auto note = message.getNoteNumber() + semitones;

                    if (! juce::isPositiveAndBelow(note, 128))
                        continue;

                    message.setNoteNumber(note);
                    layer.addEvent(message);
                }

                if (layer.getNumEvents() == 0)
                    return false;

                for (auto i = 0; i < layer.getNumEvents(); ++i)
                    if (const auto* event = layer.getEventPointer(i))
                        clip.sequence.addEvent(event->message);

                clip.sequence.sort();
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

bool AudioEngine::setMidiClipVelocity(const TrackId& trackId, const juce::Uuid& clipId, float velocity)
{
    if (! std::isfinite(velocity))
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                const auto safeVelocity = juce::jlimit(0.01f, 1.0f, velocity);
                auto changed = false;

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr || ! event->message.isNoteOn())
                        continue;

                    event->message.setVelocity(safeVelocity);
                    changed = true;
                }

                if (changed)
                    clip.sequence.updateMatchedPairs();

                return changed;
            }

    return false;
}

bool AudioEngine::humanizeMidiClipVelocity(const TrackId& trackId,
                                           const juce::Uuid& clipId,
                                           float amount)
{
    if (! std::isfinite(amount) || amount <= 0.0f)
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto changed = false;
                auto& random = juce::Random::getSystemRandom();
                const auto safeAmount = juce::jlimit(0.0f, 0.5f, amount);

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr || ! event->message.isNoteOn())
                        continue;

                    const auto offset = random.nextFloat() * safeAmount * 2.0f - safeAmount;
                    const auto velocity = juce::jlimit(0.01f,
                                                       1.0f,
                                                       event->message.getFloatVelocity() + offset);
                    event->message.setVelocity(velocity);
                    changed = true;
                }

                return changed;
            }

    return false;
}

bool AudioEngine::humanizeMidiClipTiming(const TrackId& trackId,
                                         const juce::Uuid& clipId,
                                         double amountBeats)
{
    if (! std::isfinite(amountBeats) || amountBeats <= 0.0)
        return false;

    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto changed = false;
                auto& random = juce::Random::getSystemRandom();
                const auto safeAmount = juce::jlimit(0.0, 0.25, amountBeats);

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* noteOnEvent = clip.sequence.getEventPointer(i);

                    if (noteOnEvent == nullptr || ! noteOnEvent->message.isNoteOn())
                        continue;

                    auto* noteOffEvent = static_cast<juce::MidiMessageSequence::MidiEventHolder*>(nullptr);

                    for (auto next = i + 1; next < clip.sequence.getNumEvents(); ++next)
                    {
                        auto* candidate = clip.sequence.getEventPointer(next);

                        if (candidate == nullptr || ! candidate->message.isNoteOff())
                            continue;

                        if (candidate->message.getNoteNumber() == noteOnEvent->message.getNoteNumber()
                            && candidate->message.getChannel() == noteOnEvent->message.getChannel())
                        {
                            noteOffEvent = candidate;
                            break;
                        }
                    }

                    if (noteOffEvent == nullptr)
                        continue;

                    const auto startBeat = noteOnEvent->message.getTimeStamp();
                    const auto endBeat = noteOffEvent->message.getTimeStamp();
                    const auto duration = juce::jmax(0.05, endBeat - startBeat);
                    const auto offset = (random.nextDouble() * 2.0 - 1.0) * safeAmount;
                    const auto newStart = juce::jlimit(0.0,
                                                       juce::jmax(0.0, clip.lengthBeats - duration),
                                                       startBeat + offset);
                    const auto newEnd = juce::jlimit(newStart + 0.05,
                                                     clip.lengthBeats,
                                                     newStart + duration);

                    noteOnEvent->message.setTimeStamp(newStart);
                    noteOffEvent->message.setTimeStamp(newEnd);
                    changed = true;
                }

                if (changed)
                    clip.sequence.updateMatchedPairs();

                return changed;
            }

    return false;
}

bool AudioEngine::legatoMidiClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto changed = false;

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* noteOnEvent = clip.sequence.getEventPointer(i);

                    if (noteOnEvent == nullptr || ! noteOnEvent->message.isNoteOn())
                        continue;

                    const auto noteNumber = noteOnEvent->message.getNoteNumber();
                    const auto channel = noteOnEvent->message.getChannel();
                    const auto startBeat = noteOnEvent->message.getTimeStamp();
                    auto targetEndBeat = clip.lengthBeats;

                    for (auto next = i + 1; next < clip.sequence.getNumEvents(); ++next)
                    {
                        const auto* nextEvent = clip.sequence.getEventPointer(next);

                        if (nextEvent == nullptr || ! nextEvent->message.isNoteOn())
                            continue;

                        targetEndBeat = juce::jlimit(startBeat + 0.05,
                                                     clip.lengthBeats,
                                                     nextEvent->message.getTimeStamp());
                        break;
                    }

                    for (auto next = i + 1; next < clip.sequence.getNumEvents(); ++next)
                    {
                        auto* noteOffEvent = clip.sequence.getEventPointer(next);

                        if (noteOffEvent == nullptr || ! noteOffEvent->message.isNoteOff())
                            continue;

                        if (noteOffEvent->message.getNoteNumber() == noteNumber
                            && noteOffEvent->message.getChannel() == channel)
                        {
                            noteOffEvent->message.setTimeStamp(targetEndBeat);
                            changed = true;
                            break;
                        }
                    }
                }

                if (changed)
                    clip.sequence.updateMatchedPairs();

                return changed;
            }

    return false;
}

bool AudioEngine::staccatoMidiClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.sequence.updateMatchedPairs();
                auto changed = false;

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    auto* noteOnEvent = clip.sequence.getEventPointer(i);

                    if (noteOnEvent == nullptr || ! noteOnEvent->message.isNoteOn())
                        continue;

                    auto* noteOffEvent = noteOnEvent->noteOffObject;

                    if (noteOffEvent == nullptr)
                        continue;

                    const auto startBeat = noteOnEvent->message.getTimeStamp();
                    const auto currentEndBeat = noteOffEvent->message.getTimeStamp();
                    const auto targetEndBeat = juce::jlimit(startBeat + 0.05,
                                                           currentEndBeat,
                                                           startBeat + 0.25);

                    if (targetEndBeat < currentEndBeat)
                    {
                        noteOffEvent->message.setTimeStamp(targetEndBeat);
                        changed = true;
                    }
                }

                if (changed)
                    clip.sequence.updateMatchedPairs();

                return changed;
            }

    return false;
}

bool AudioEngine::reverseMidiClip(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                if (clip.lengthBeats <= 0.0)
                    return false;

                clip.sequence.updateMatchedPairs();
                juce::MidiMessageSequence reversedSequence;

                const auto addAt = [](juce::MidiMessageSequence& sequence,
                                      juce::MidiMessage message,
                                      double beat)
                {
                    message.setTimeStamp(juce::jmax(0.0, beat));
                    sequence.addEvent(message);
                };

                for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
                {
                    const auto* event = clip.sequence.getEventPointer(i);

                    if (event == nullptr)
                        continue;

                    const auto message = event->message;
                    const auto timeStamp = message.getTimeStamp();

                    if (message.isNoteOn())
                    {
                        const auto endBeat = event->noteOffObject != nullptr
                            ? event->noteOffObject->message.getTimeStamp()
                            : juce::jmin(clip.lengthBeats, timeStamp + 0.25);
                        const auto reversedStart = juce::jlimit(0.0, clip.lengthBeats, clip.lengthBeats - endBeat);
                        const auto reversedEnd = juce::jlimit(reversedStart + 0.05,
                                                             clip.lengthBeats,
                                                             clip.lengthBeats - timeStamp);

                        addAt(reversedSequence, message, reversedStart);
                        addAt(reversedSequence,
                              juce::MidiMessage::noteOff(message.getChannel(), message.getNoteNumber()),
                              reversedEnd);
                    }
                    else if (! message.isNoteOff())
                    {
                        addAt(reversedSequence, message, clip.lengthBeats - timeStamp);
                    }
                }

                clip.sequence = reversedSequence;
                clip.sequence.sort();
                clip.sequence.updateMatchedPairs();
                return true;
            }

    return false;
}

bool AudioEngine::toggleMidiClipMuted(const TrackId& trackId, const juce::Uuid& clipId)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
        for (auto& clip : track->clips)
            if (clip.id == clipId)
            {
                clip.muted = ! clip.muted;
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

bool AudioEngine::generateBassline(const TrackId& trackId, const juce::String& style)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
    {
        track->clips.push_back(createBasslineClip(style));
        return true;
    }

    return false;
}

bool AudioEngine::generateArpeggio(const TrackId& trackId, const juce::String& style)
{
    std::scoped_lock lock(modelMutex);
    saveUndoSnapshotNoLock();

    if (auto* track = projectModel.findMidiTrack(trackId))
    {
        track->clips.push_back(createArpeggioClip(style));
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
        applyMonoMonitoring(buffer);

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

bool AudioEngine::exportTrackToWav(const TrackId& trackId, const juce::File& destinationFile)
{
    {
        std::scoped_lock lock(modelMutex);

        if (projectModel.findAudioTrack(trackId) == nullptr
            && projectModel.findMidiTrack(trackId) == nullptr)
        {
            return false;
        }
    }

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
            renderAudioTracks(buffer,
                              numToRender,
                              static_cast<double>(rendered) / outputSampleRate,
                              &trackId);
            renderMidiTracks(buffer,
                             numToRender,
                             static_cast<double>(rendered) / outputSampleRate,
                             exportMidi,
                             exportSynth,
                             &trackId);
        }

        buffer.applyGain(masterGain.load());
        applyMonoMonitoring(buffer);

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

void AudioEngine::setMonoMonitoringEnabled(bool enabled)
{
    monoMonitoringEnabled.store(enabled);
}

bool AudioEngine::isMonoMonitoringEnabled() const
{
    return monoMonitoringEnabled.load();
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
    applyMonoMonitoring(renderBuffer);
    auto peak = 0.0f;

    for (auto channel = 0; channel < renderBuffer.getNumChannels(); ++channel)
        peak = juce::jmax(peak, renderBuffer.getMagnitude(channel, 0, numSamples));

    lastOutputPeak.store(peak);
    auto heldPeak = heldOutputPeak.load();

    while (peak > heldPeak && ! heldOutputPeak.compare_exchange_weak(heldPeak, peak))
    {
    }

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
                                    double blockStartSeconds,
                                    const TrackId* onlyTrackId) const
{
    const auto anySolo = onlyTrackId == nullptr ? anySoloedTrack() : false;
    const auto outputSampleRate = currentSampleRate.load();

    for (const auto& trackPtr : projectModel.getAudioTracks())
    {
        const auto& track = *trackPtr;

        if (onlyTrackId != nullptr && track.state.id != *onlyTrackId)
            continue;

        if (! track.hasAudio() || ! shouldRenderTrack(track.state, anySolo))
            continue;

        if (track.clips.empty())
            continue;

        for (const auto& clip : track.clips)
        {
            if (clip.muted)
                continue;

            for (auto sample = 0; sample < numSamples; ++sample)
            {
                const auto timeSeconds = blockStartSeconds + static_cast<double>(sample) / outputSampleRate;
                const auto clipTime = timeSeconds - clip.startTimeSeconds;

                if (clipTime < 0.0 || clipTime >= clip.lengthSeconds)
                    continue;

                const auto sourceTime = clip.reversed
                    ? clip.sourceOffsetSeconds
                        + juce::jmax(0.0, clip.lengthSeconds - clipTime - (1.0 / track.sampleRate))
                    : clipTime + clip.sourceOffsetSeconds;
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
                                   SimpleSynth& synthToUse,
                                   const TrackId* onlyTrackId) const
{
    const auto anySolo = onlyTrackId == nullptr ? anySoloedTrack() : false;
    const auto sampleRate = currentSampleRate.load();
    const auto& tempo = projectModel.getTempoMap();

    for (const auto& trackPtr : projectModel.getMidiTracks())
    {
        const auto& track = *trackPtr;

        if (onlyTrackId != nullptr && track.state.id != *onlyTrackId)
            continue;

        if (! shouldRenderTrack(track.state, anySolo))
            continue;

        for (const auto& clip : track.clips)
        {
            if (clip.muted)
                continue;

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

void AudioEngine::applyMonoMonitoring(juce::AudioBuffer<float>& buffer) const
{
    if (! monoMonitoringEnabled.load() || buffer.getNumChannels() < 2)
        return;

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto mono = (buffer.getSample(0, sample) + buffer.getSample(1, sample)) * 0.5f;

        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, mono);
    }
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

MidiClip AudioEngine::createBasslineClip(const juce::String& style) const
{
    const auto normalized = style.toLowerCase();
    const std::array<int, 4> popRoots { 36, 43, 45, 41 };
    const std::array<int, 4> minorRoots { 33, 41, 36, 40 };
    const auto& roots = normalized.contains("minor") ? minorRoots : popRoots;
    const std::array<double, 8> rhythm { 0.0, 0.5, 1.5, 2.0, 2.5, 3.0, 3.5, 3.75 };

    MidiClip clip;
    clip.id = juce::Uuid();
    clip.startBeat = projectModel.getTempoMap().secondsToBeats(getPosition());
    clip.lengthBeats = 16.0;

    for (auto bar = 0; bar < static_cast<int>(roots.size()); ++bar)
    {
        const auto root = roots[static_cast<size_t>(bar)];
        const auto barStart = static_cast<double>(bar) * 4.0;

        for (auto step = 0; step < static_cast<int>(rhythm.size()); ++step)
        {
            const auto beat = barStart + rhythm[static_cast<size_t>(step)];
            const auto note = (step % 4 == 2) ? root + 7 : root;
            auto noteOn = juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(86));
            noteOn.setTimeStamp(beat);
            auto noteOff = juce::MidiMessage::noteOff(1, note);
            noteOff.setTimeStamp(juce::jmin(barStart + 4.0, beat + 0.35));
            clip.sequence.addEvent(noteOn);
            clip.sequence.addEvent(noteOff);
        }
    }

    clip.sequence.updateMatchedPairs();
    return clip;
}

MidiClip AudioEngine::createArpeggioClip(const juce::String& style) const
{
    const auto normalized = style.toLowerCase();
    const std::array<std::array<int, 4>, 4> pop {
        std::array<int, 4> { 60, 64, 67, 72 },
        std::array<int, 4> { 67, 71, 74, 79 },
        std::array<int, 4> { 69, 72, 76, 81 },
        std::array<int, 4> { 65, 69, 72, 77 }
    };
    const std::array<std::array<int, 4>, 4> minor {
        std::array<int, 4> { 57, 60, 64, 69 },
        std::array<int, 4> { 65, 69, 72, 77 },
        std::array<int, 4> { 52, 55, 59, 64 },
        std::array<int, 4> { 64, 67, 71, 76 }
    };
    const auto& chords = normalized.contains("minor") ? minor : pop;

    MidiClip clip;
    clip.id = juce::Uuid();
    clip.startBeat = projectModel.getTempoMap().secondsToBeats(getPosition());
    clip.lengthBeats = 16.0;

    for (auto bar = 0; bar < static_cast<int>(chords.size()); ++bar)
    {
        const auto barStart = static_cast<double>(bar) * 4.0;
        const auto& chord = chords[static_cast<size_t>(bar)];

        for (auto step = 0; step < 16; ++step)
        {
            const auto beat = barStart + static_cast<double>(step) * 0.25;
            const auto note = chord[static_cast<size_t>(step % static_cast<int>(chord.size()))];
            auto noteOn = juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(78));
            noteOn.setTimeStamp(beat);
            auto noteOff = juce::MidiMessage::noteOff(1, note);
            noteOff.setTimeStamp(beat + 0.18);
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
