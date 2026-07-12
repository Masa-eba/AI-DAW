#include "SimpleSynth.h"

bool SimpleSynthSound::appliesToNote(int)
{
    return true;
}

bool SimpleSynthSound::appliesToChannel(int)
{
    return true;
}

bool SimpleSynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SimpleSynthSound*>(sound) != nullptr;
}

void SimpleSynthVoice::startNote(int midiNoteNumber,
                                 float velocity,
                                 juce::SynthesiserSound*,
                                 int)
{
    const auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    const auto cyclesPerSample = cyclesPerSecond / getSampleRate();
    angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    level = velocity * 0.25f;
    adsr.setSampleRate(getSampleRate());
    adsr.setParameters(adsrParameters);
    adsr.noteOn();
}

void SimpleSynthVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
        return;
    }

    clearCurrentNote();
    angleDelta = 0.0;
    adsr.reset();
}

void SimpleSynthVoice::pitchWheelMoved(int)
{
}

void SimpleSynthVoice::controllerMoved(int, int)
{
}

void SimpleSynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                       int startSample,
                                       int numSamples)
{
    if (angleDelta == 0.0)
        return;

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto currentSample = static_cast<float>(std::sin(currentAngle)) * level * adsr.getNextSample();

        for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, currentSample);

        currentAngle += angleDelta;

        if (! adsr.isActive())
        {
            clearCurrentNote();
            angleDelta = 0.0;
            break;
        }
    }
}

SimpleSynth::SimpleSynth()
{
    for (auto i = 0; i < 16; ++i)
        synth.addVoice(new SimpleSynthVoice());

    synth.addSound(new SimpleSynthSound());
}

void SimpleSynth::setCurrentPlaybackSampleRate(double sampleRate)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void SimpleSynth::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midiMessages,
                                  int startSample,
                                  int numSamples)
{
    synth.renderNextBlock(buffer, midiMessages, startSample, numSamples);
}
