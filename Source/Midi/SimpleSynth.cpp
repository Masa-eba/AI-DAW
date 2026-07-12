#include "SimpleSynth.h"

#include <cmath>

SimpleSynthSound::SimpleSynthSound(int midiChannel, SimpleSynthInstrument instrumentType)
    : channel(midiChannel),
      instrument(instrumentType)
{
}

bool SimpleSynthSound::appliesToNote(int)
{
    return true;
}

bool SimpleSynthSound::appliesToChannel(int midiChannel)
{
    return midiChannel == channel;
}

SimpleSynthInstrument SimpleSynthSound::getInstrument() const
{
    return instrument;
}

bool SimpleSynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SimpleSynthSound*>(sound) != nullptr;
}

void SimpleSynthVoice::startNote(int midiNoteNumber,
                                 float velocity,
                                 juce::SynthesiserSound* sound,
                                 int)
{
    instrument = SimpleSynthInstrument::Lead;

    if (const auto* synthSound = dynamic_cast<SimpleSynthSound*>(sound))
        instrument = synthSound->getInstrument();

    const auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    const auto cyclesPerSample = cyclesPerSecond / getSampleRate();
    angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    currentAngle = 0.0;
    currentMidiNote = midiNoteNumber;
    noiseState = static_cast<uint32_t>(midiNoteNumber) * 2654435761u;

    switch (instrument)
    {
        case SimpleSynthInstrument::Bass:
            adsrParameters = { 0.002f, 0.08f, 0.65f, 0.08f };
            level = velocity * 0.28f;
            break;

        case SimpleSynthInstrument::Guitar:
            adsrParameters = { 0.001f, 0.12f, 0.12f, 0.18f };
            level = velocity * 0.24f;
            break;

        case SimpleSynthInstrument::Drum:
            adsrParameters = { 0.001f, 0.04f, 0.0f, 0.06f };
            level = velocity * 0.35f;
            break;

        case SimpleSynthInstrument::Lead:
        default:
            adsrParameters = { 0.01f, 0.1f, 0.7f, 0.18f };
            level = velocity * 0.22f;
            break;
    }

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

    const auto nextNoise = [this]()
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return (static_cast<float>((noiseState >> 9) & 0x7fffff) / 4194304.0f) - 1.0f;
    };

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto envelope = adsr.getNextSample();
        auto oscillatorSample = 0.0f;

        switch (instrument)
        {
            case SimpleSynthInstrument::Bass:
                oscillatorSample = std::tanh(static_cast<float>(std::sin(currentAngle) * 2.8));
                break;

            case SimpleSynthInstrument::Guitar:
                oscillatorSample = static_cast<float>(std::sin(currentAngle)
                                                       + 0.35 * std::sin(currentAngle * 2.0)
                                                       + 0.16 * std::sin(currentAngle * 3.0));
                oscillatorSample = std::tanh(oscillatorSample * 1.5f);
                break;

            case SimpleSynthInstrument::Drum:
                if (currentMidiNote == 36)
                {
                    oscillatorSample = static_cast<float>(std::sin(currentAngle)) * 1.2f;
                    angleDelta *= 0.9994;
                }
                else if (currentMidiNote == 42)
                {
                    oscillatorSample = nextNoise() * 0.45f;
                }
                else
                {
                    oscillatorSample = static_cast<float>(std::sin(currentAngle)) * 0.55f + nextNoise() * 0.65f;
                }
                break;

            case SimpleSynthInstrument::Lead:
            default:
                oscillatorSample = static_cast<float>(std::sin(currentAngle)
                                                       + 0.22 * std::sin(currentAngle * 2.0));
                break;
        }

        const auto currentSample = oscillatorSample * level * envelope;

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
    for (auto i = 0; i < 24; ++i)
        synth.addVoice(new SimpleSynthVoice());

    synth.addSound(new SimpleSynthSound(1, SimpleSynthInstrument::Lead));
    synth.addSound(new SimpleSynthSound(2, SimpleSynthInstrument::Bass));
    synth.addSound(new SimpleSynthSound(3, SimpleSynthInstrument::Guitar));
    synth.addSound(new SimpleSynthSound(10, SimpleSynthInstrument::Drum));
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

void SimpleSynth::allNotesOff()
{
    for (auto channel = 1; channel <= 16; ++channel)
        synth.allNotesOff(channel, false);
}
