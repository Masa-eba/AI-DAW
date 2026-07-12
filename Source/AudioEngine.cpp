#include "AudioEngine.h"

#include <cmath>
#include <memory>

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
    return projectModel.addAudioTrack();
}

TrackId AudioEngine::addMidiTrack()
{
    std::scoped_lock lock(modelMutex);
    return projectModel.addMidiTrack();
}

bool AudioEngine::removeTrack(const TrackId& trackId)
{
    std::scoped_lock lock(modelMutex);
    return projectModel.removeTrack(trackId);
}

bool AudioEngine::loadFile(const juce::File& file)
{
    std::scoped_lock lock(modelMutex);

    if (projectModel.getAudioTracks().empty())
        projectModel.addAudioTrack();

    return loadAudioFileIntoTrack(*projectModel.getAudioTracks().front(), file);
}

bool AudioEngine::loadAudioFileToTrack(const TrackId& trackId, const juce::File& file)
{
    std::scoped_lock lock(modelMutex);

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
}

void AudioEngine::stop()
{
    if (isRecording())
        stopRecording();

    transportState.store(TransportState::Stopped);
    transportSamplePosition.store(0);
    metronome.reset(0.0);
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
        loadAudioFileIntoTrack(*track, lastRecordingFile);
        lastRecordingFile = juce::File();
    }

    if (recordingMidi.exchange(false))
    {
        if (auto* track = getFirstArmedMidiTrack())
        {
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

void AudioEngine::setTrackMuted(const TrackId& trackId, bool muted)
{
    std::scoped_lock lock(modelMutex);

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.muted = muted;
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.muted = muted;
}

void AudioEngine::setTrackSolo(const TrackId& trackId, bool solo)
{
    std::scoped_lock lock(modelMutex);

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.solo = solo;
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.solo = solo;
}

void AudioEngine::setTrackArmed(const TrackId& trackId, bool armed)
{
    std::scoped_lock lock(modelMutex);

    for (auto& track : projectModel.getAudioTracks())
        track->state.armed = false;

    for (auto& track : projectModel.getMidiTracks())
        track->state.armed = false;

    if (auto* track = projectModel.findAudioTrack(trackId))
        track->state.armed = armed;
    else if (auto* midiTrack = projectModel.findMidiTrack(trackId))
        midiTrack->state.armed = armed;
}

void AudioEngine::setAudioClipStartTime(const TrackId& trackId, double startTimeSeconds)
{
    if (! std::isfinite(startTimeSeconds) || startTimeSeconds < 0.0)
        startTimeSeconds = 0.0;

    std::scoped_lock lock(modelMutex);

    if (auto* track = projectModel.findAudioTrack(trackId); track != nullptr && ! track->clips.empty())
        track->clips.front().startTimeSeconds = startTimeSeconds;
}

void AudioEngine::moveAudioClipToTrack(const TrackId& sourceTrackId,
                                       const TrackId& destinationTrackId,
                                       double startTimeSeconds)
{
    if (! std::isfinite(startTimeSeconds) || startTimeSeconds < 0.0)
        startTimeSeconds = 0.0;

    std::scoped_lock lock(modelMutex);

    auto* sourceTrack = projectModel.findAudioTrack(sourceTrackId);
    auto* destinationTrack = projectModel.findAudioTrack(destinationTrackId);

    if (sourceTrack == nullptr || destinationTrack == nullptr || ! sourceTrack->hasAudio())
        return;

    if (sourceTrack == destinationTrack)
    {
        if (! sourceTrack->clips.empty())
            sourceTrack->clips.front().startTimeSeconds = startTimeSeconds;

        return;
    }

    destinationTrack->audioBuffer = std::move(sourceTrack->audioBuffer);
    destinationTrack->sampleRate = sourceTrack->sampleRate;
    destinationTrack->clips = std::move(sourceTrack->clips);

    if (! destinationTrack->clips.empty())
        destinationTrack->clips.front().startTimeSeconds = startTimeSeconds;

    sourceTrack->audioBuffer.setSize(0, 0);
    sourceTrack->sampleRate = 0.0;
    sourceTrack->clips.clear();
}

void AudioEngine::setMidiKeyboardState(juce::MidiKeyboardState* state)
{
    keyboardState = state;
}

bool AudioEngine::saveProject(const juce::File& file) const
{
    std::scoped_lock lock(modelMutex);
    return projectModel.saveToFile(file);
}

bool AudioEngine::loadProject(const juce::File& file)
{
    std::scoped_lock lock(modelMutex);
    return projectModel.loadFromFile(file);
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

        if (getLength() > 0.0 && getPosition() >= getLength())
            stop();
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
    track.clips.clear();
    AudioClip clip;
    clip.sourceFile = file;
    clip.lengthSeconds = track.getAudioDurationSeconds();
    track.clips.push_back(clip);
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

        const auto& clip = track.clips.front();

        for (auto sample = 0; sample < numSamples; ++sample)
        {
            const auto timeSeconds = blockStartSeconds + static_cast<double>(sample) / outputSampleRate;
            const auto sourceTime = timeSeconds - clip.startTimeSeconds + clip.sourceOffsetSeconds;

            if (sourceTime < 0.0 || sourceTime >= clip.lengthSeconds)
                continue;

            const auto sourceIndex = static_cast<int>(sourceTime * track.sampleRate);

            if (sourceIndex < 0 || sourceIndex >= track.audioBuffer.getNumSamples())
                continue;

            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto sourceChannel = juce::jmin(channel, track.audioBuffer.getNumChannels() - 1);
                buffer.addSample(channel,
                                 sample,
                                 track.audioBuffer.getSample(sourceChannel, sourceIndex)
                                     * track.state.gain);
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

                const auto eventSeconds = tempo.beatsToSeconds(clip.startBeat
                                             + event->message.getTimeStamp());
                const auto offset = static_cast<int>((eventSeconds - blockStartSeconds) * sampleRate);

                if (offset >= 0 && offset < numSamples)
                    midiMessages.addEvent(event->message, offset);
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
