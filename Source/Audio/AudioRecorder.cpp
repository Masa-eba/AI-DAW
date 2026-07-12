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
    writerChannels = numChannels;
    writeBuffer.setSize(writerChannels, 16384, false, false, true);
    writeBuffer.clear();
    currentFile = destination;
    activeWriter.store(threadedWriter.get());
    return true;
}

void AudioRecorder::stopRecording()
{
    activeWriter.store(nullptr);
    threadedWriter.reset();
    writerChannels = 0;
}

void AudioRecorder::processInput(const float* const* inputChannelData,
                                 int numInputChannels,
                                 int numSamples)
{
    auto* writer = activeWriter.load();

    if (writer == nullptr || writerChannels <= 0 || numSamples <= 0)
        return;

    auto samplesWritten = 0;

    while (samplesWritten < numSamples)
    {
        const auto samplesThisBlock = juce::jmin(numSamples - samplesWritten,
                                                 writeBuffer.getNumSamples());
        writeBuffer.clear(0, samplesThisBlock);

        for (auto channel = 0; channel < writerChannels; ++channel)
        {
            const auto sourceChannel = numInputChannels > 0
                ? juce::jmin(channel, numInputChannels - 1)
                : -1;
            const auto* source = sourceChannel >= 0 && inputChannelData != nullptr
                ? inputChannelData[sourceChannel]
                : nullptr;

            if (source != nullptr)
                writeBuffer.copyFrom(channel, 0, source + samplesWritten, samplesThisBlock);
        }

        std::array<const float*, 8> channelData {};

        for (auto channel = 0; channel < writerChannels && channel < static_cast<int>(channelData.size()); ++channel)
            channelData[static_cast<size_t>(channel)] = writeBuffer.getReadPointer(channel);

        writer->write(channelData.data(), samplesThisBlock);
        samplesWritten += samplesThisBlock;
    }
}

bool AudioRecorder::isRecording() const
{
    return activeWriter.load() != nullptr;
}

juce::File AudioRecorder::getCurrentFile() const
{
    return currentFile;
}
