#pragma once

#include <JuceHeader.h>

class SimpleSynthSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override;
    bool appliesToChannel(int) override;
};

class SimpleSynthVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override;
    void controllerMoved(int, int) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParameters { 0.01f, 0.1f, 0.8f, 0.2f };
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    float level = 0.0f;
};

class SimpleSynth final
{
public:
    SimpleSynth();

    void setCurrentPlaybackSampleRate(double sampleRate);
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midiMessages,
                         int startSample,
                         int numSamples);

private:
    juce::Synthesiser synth;
};
