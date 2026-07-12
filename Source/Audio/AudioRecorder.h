#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>

class AudioRecorder final
{
public:
    AudioRecorder();
    ~AudioRecorder();

    bool startRecording(const juce::File& destination, double sampleRate, int numChannels);
    void stopRecording();
    void processInput(const float* const* inputChannelData, int numInputChannels, int numSamples);
    bool isRecording() const;
    juce::File getCurrentFile() const;

private:
    juce::TimeSliceThread writerThread { "Audio Recorder Writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::File currentFile;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
};
