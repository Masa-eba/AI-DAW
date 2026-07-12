#pragma once

#include "Audio/AudioRecorder.h"
#include "Audio/Metronome.h"
#include "Core/ProjectModel.h"
#include "Midi/SimpleSynth.h"

#include <JuceHeader.h>

#include <atomic>
#include <mutex>

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
    bool removeTrack(const TrackId& trackId);
    bool loadFile(const juce::File& file);
    bool loadAudioFileToTrack(const TrackId& trackId, const juce::File& file);

    void play();
    void pause();
    void stop();
    bool startRecording();
    void stopRecording();

    void setPosition(double seconds);
    double getPosition() const;
    double getLength() const;
    void setBpm(double bpm);
    double getBpm() const;
    void setGain(float gain);
    float getGain() const;
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const;
    void setLoopEnabled(bool enabled);
    bool isLoopEnabled() const;
    bool isPlaying() const;
    bool isRecording() const;
    bool hasLoadedFile() const;
    const juce::File& getCurrentFile() const;

    void setTrackGain(const TrackId& trackId, float gain);
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
    bool duplicateAudioClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool deleteAudioClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool splitAudioClipAtPosition(const TrackId& trackId, const juce::Uuid& clipId, double positionSeconds);
    bool setAudioClipFade(const TrackId& trackId,
                          const juce::Uuid& clipId,
                          double fadeInSeconds,
                          double fadeOutSeconds);
    void setMidiClipStartBeat(const TrackId& trackId, const juce::Uuid& clipId, double startBeat);
    void moveMidiClipToTrack(const TrackId& sourceTrackId,
                             const TrackId& destinationTrackId,
                             const juce::Uuid& clipId,
                             double startBeat);
    bool duplicateMidiClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool deleteMidiClip(const TrackId& trackId, const juce::Uuid& clipId);
    bool generateChordProgression(const TrackId& trackId, const juce::String& style);

    void setMidiKeyboardState(juce::MidiKeyboardState* state);
    bool saveProject(const juce::File& file) const;
    bool loadProject(const juce::File& file);
    bool exportToWav(const juce::File& destinationFile);

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
    std::atomic<bool> recordingMidi { false };
    juce::MidiMessageSequence activeRecordingSequence;
    double recordingStartBeats = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
