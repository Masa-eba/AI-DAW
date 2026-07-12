#pragma once

#include "Audio/AudioRecorder.h"
#include "Audio/Metronome.h"
#include "Core/ProjectModel.h"
#include "Midi/SimpleSynth.h"

#include <JuceHeader.h>

#include <atomic>
#include <mutex>
#include <vector>

enum class TransportState
{
    Stopped,
    Playing,
    Paused,
    Recording
};

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    ProjectModel& getProjectModel();
    const ProjectModel& getProjectModel() const;

    TrackId addAudioTrack();
    TrackId addMidiTrack();
    TrackId duplicateTrack(const TrackId& trackId);
    bool moveTrack(const TrackId& trackId, int direction);
    bool removeTrack(const TrackId& trackId);
    void newProject();
    bool loadFile(const juce::File& file);
    bool loadAudioFileToTrack(const TrackId& trackId, const juce::File& file);

    void play();
    void pause();
    void stop();
    void panic();
    bool startRecording();
    void stopRecording();

    void setPosition(double seconds);
    double getPosition() const;
    double getLength() const;
    void setBpm(double bpm);
    double getBpm() const;
    juce::Uuid addMarker(double timeSeconds);
    bool renameMarker(const juce::Uuid& markerId, const juce::String& name);
    bool removeNearestMarker(double timeSeconds, double thresholdSeconds);
    void setGain(float gain);
    float getGain() const;
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const;
    void setLoopEnabled(bool enabled);
    void setLoopRange(double startSeconds, double endSeconds);
    void clearLoopRange();
    bool isLoopEnabled() const;
    bool isPlaying() const;
    bool isRecording() const;
    bool hasLoadedFile() const;
    const juce::File& getCurrentFile() const;

    void setTrackGain(const TrackId& trackId, float gain);
    void setTrackPan(const TrackId& trackId, float pan);
    bool renameTrack(const TrackId& trackId, const juce::String& newName);
    void setTrackMuted(const TrackId& trackId, bool muted);
    void setTrackSolo(const TrackId& trackId, bool solo);
    void setTrackArmed(const TrackId& trackId, bool armed);
    void setAudioClipStartTime(const TrackId& trackId,
                               const juce::Uuid& clipId,
                               double startTimeSeconds);
    void moveAudioClipToTrack(const TrackId& sourceTrackId,
                              const TrackId& destinationTrackId,
                              const juce::Uuid& clipId,
                              double startTimeSeconds);
    bool setAudioClipTiming(const TrackId& trackId,
                            const juce::Uuid& clipId,
                            double startTimeSeconds,
                            double sourceOffsetSeconds,
                            double lengthSeconds);
    bool duplicateAudioClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool duplicateAudioClipAtTime(const TrackId& trackId,
                                  const juce::Uuid& clipId,
                                  double startTimeSeconds);
    bool deleteAudioClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool splitAudioClipAtPosition(const TrackId& trackId, const juce::Uuid& clipId, double positionSeconds);
    bool setAudioClipFade(const TrackId& trackId,
                          const juce::Uuid& clipId,
                          double fadeInSeconds,
                          double fadeOutSeconds);
    bool adjustAudioClipGain(const TrackId& trackId, const juce::Uuid& clipId, float delta);
    bool normalizeAudioClipGain(const TrackId& trackId, const juce::Uuid& clipId);
    bool toggleAudioClipMuted(const TrackId& trackId, const juce::Uuid& clipId);
    bool toggleAudioClipReversed(const TrackId& trackId, const juce::Uuid& clipId);
    void setMidiClipStartBeat(const TrackId& trackId, const juce::Uuid& clipId, double startBeat);
    bool setMidiClipLength(const TrackId& trackId, const juce::Uuid& clipId, double lengthBeats);
    void moveMidiClipToTrack(const TrackId& sourceTrackId,
                             const TrackId& destinationTrackId,
                             const juce::Uuid& clipId,
                             double startBeat);
    bool duplicateMidiClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool duplicateMidiClipAtBeat(const TrackId& trackId,
                                 const juce::Uuid& clipId,
                                 double startBeat);
    bool deleteMidiClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool quantizeMidiClip(const TrackId& trackId, const juce::Uuid& clipId, double gridBeats);
    bool transposeMidiClip(const TrackId& trackId, const juce::Uuid& clipId, int semitones);
    bool adjustMidiClipVelocity(const TrackId& trackId, const juce::Uuid& clipId, float delta);
    bool humanizeMidiClipVelocity(const TrackId& trackId, const juce::Uuid& clipId, float amount);
    bool humanizeMidiClipTiming(const TrackId& trackId, const juce::Uuid& clipId, double amountBeats);
    bool legatoMidiClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool toggleMidiClipMuted(const TrackId& trackId, const juce::Uuid& clipId);
    bool generateChordProgression(const TrackId& trackId, const juce::String& style);

    void setMidiKeyboardState(juce::MidiKeyboardState* state);
    bool saveProject(const juce::File& file);
    bool loadProject(const juce::File& file);
    bool exportToWav(const juce::File& destinationFile);
    bool undo();
    bool redo();

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    bool loadAudioFileIntoTrack(AudioTrack& track, const juce::File& file);
    bool loadAudioBufferForTrack(AudioTrack& track, const juce::File& file);
    void renderToBuffer(juce::AudioBuffer<float>& buffer,
                        int numSamples,
                        double blockStartSeconds,
                        bool includeMetronome);
    void renderAudioTracks(juce::AudioBuffer<float>& buffer,
                           int numSamples,
                           double blockStartSeconds) const;
    void renderMidiTracks(juce::AudioBuffer<float>& buffer,
                          int numSamples,
                          double blockStartSeconds,
                          juce::MidiBuffer& midiBuffer,
                          SimpleSynth& synthToUse) const;
    bool anySoloedTrack() const;
    bool shouldRenderTrack(const TrackState& state, bool anySolo) const;
    MidiClip createChordProgressionClip(const juce::String& style) const;
    AudioTrack* getFirstArmedAudioTrack();
    MidiTrack* getFirstArmedMidiTrack();
    void saveUndoSnapshotNoLock();
    bool restoreProjectSnapshotNoLock(const juce::String& snapshot);

    mutable std::mutex modelMutex;
    ProjectModel projectModel;
    juce::AudioFormatManager formatManager;
    juce::File currentFile;
    juce::File lastRecordingFile;
    juce::AudioBuffer<float> renderBuffer;
    juce::MidiBuffer midiBuffer;
    SimpleSynth synth;
    Metronome metronome;
    AudioRecorder recorder;
    juce::MidiKeyboardState* keyboardState = nullptr;

    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int64_t> transportSamplePosition { 0 };
    std::atomic<TransportState> transportState { TransportState::Stopped };
    std::atomic<float> masterGain { 0.8f };
    std::atomic<bool> loopEnabled { false };
    std::atomic<double> loopStartSeconds { 0.0 };
    std::atomic<double> loopEndSeconds { 0.0 };
    std::atomic<bool> recordingMidi { false };
    juce::MidiMessageSequence activeRecordingSequence;
    double recordingStartBeats = 0.0;
    std::vector<juce::String> undoStack;
    std::vector<juce::String> redoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
