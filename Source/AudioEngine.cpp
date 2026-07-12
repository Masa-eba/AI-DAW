#include "AudioEngine.h"

#include <cmath>
#include <memory>

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    setGain(0.8f);
}

AudioEngine::~AudioEngine()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
}

bool AudioEngine::loadFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
        return false;

    const auto sourceSampleRate = reader->sampleRate;
    auto newReaderSource =
        std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();

    transportSource.setSource(newReaderSource.get(), 0, nullptr, sourceSampleRate);
    readerSource = std::move(newReaderSource);
    currentFile = file;
    transportSource.setPosition(0.0);

    return true;
}

void AudioEngine::play()
{
    if (readerSource != nullptr)
        transportSource.start();
}

void AudioEngine::pause()
{
    if (readerSource != nullptr)
        transportSource.stop();
}

void AudioEngine::stop()
{
    transportSource.stop();

    if (readerSource != nullptr)
        transportSource.setPosition(0.0);
}

void AudioEngine::setPosition(double seconds)
{
    if (readerSource == nullptr)
        return;

    if (! std::isfinite(seconds))
        seconds = 0.0;

    const auto safePosition =
        juce::jlimit(0.0, transportSource.getLengthInSeconds(), seconds);
    transportSource.setPosition(safePosition);
}

void AudioEngine::setGain(float gain)
{
    transportSource.setGain(juce::jlimit(0.0f, 1.0f, gain));
}

bool AudioEngine::isPlaying() const
{
    return transportSource.isPlaying();
}

bool AudioEngine::hasLoadedFile() const
{
    return readerSource != nullptr;
}

const juce::File& AudioEngine::getCurrentFile() const
{
    return currentFile;
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource != nullptr)
    {
        transportSource.getNextAudioBlock(bufferToFill);
        return;
    }

    bufferToFill.clearActiveBufferRegion();
}

void AudioEngine::releaseResources()
{
    transportSource.releaseResources();
}
