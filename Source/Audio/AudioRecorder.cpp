#include "AudioRecorder.h"

AudioRecorder::AudioRecorder()
{
    writerThread.startThread();
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
    writerThread.stopThread(1000);
}

bool AudioRecorder::startRecording(const juce::File& destination, double sampleRate, int numChannels)
{
    stopRecording();

    if (sampleRate <= 0.0 || numChannels <= 0)
        return false;

    if (destination.existsAsFile() && ! destination.deleteFile())
        return false;

    std::unique_ptr<juce::OutputStream> stream = destination.createOutputStream();

    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate(sampleRate)
                             .withNumChannels(numChannels)
                             .withBitsPerSample(16);
    auto writer = wavFormat.createWriterFor(stream, writerOptions);

    if (writer == nullptr)
        return false;

    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer.release(),
        writerThread,
        32768);
    currentFile = destination;
    activeWriter.store(threadedWriter.get());
    return true;
}

void AudioRecorder::stopRecording()
{
    activeWriter.store(nullptr);
    threadedWriter.reset();
}

void AudioRecorder::processInput(const float* const* inputChannelData,
                                 int numInputChannels,
                                 int numSamples)
{
    if (auto* writer = activeWriter.load())
        writer->write(inputChannelData, juce::jmax(0, numSamples));

    juce::ignoreUnused(numInputChannels);
}

bool AudioRecorder::isRecording() const
{
    return activeWriter.load() != nullptr;
}

juce::File AudioRecorder::getCurrentFile() const
{
    return currentFile;
}
