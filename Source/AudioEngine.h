#pragma once

#include <JuceHeader.h>

class AudioEngine final : public juce::AudioSource
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // Loads a supported audio file and replaces the current transport source.
    bool loadFile(const juce::File& file);

    // Starts playback from the current transport position.
    void play();

    // Stops playback while preserving the current transport position.
    void pause();

    // Stops playback and returns the transport position to the beginning.
    void stop();

    // Moves playback to a safe position in seconds.
    void setPosition(double seconds);

    // Sets output gain in the range 0.0 to 1.0.
    void setGain(float gain);

    // Returns whether the transport is currently playing.
    bool isPlaying() const;

    // Returns true after a file has been loaded successfully.
    bool hasLoadedFile() const;

    // Returns the currently loaded file, or an empty file if none is loaded.
    const juce::File& getCurrentFile() const;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

private:
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::File currentFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
