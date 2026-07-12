#pragma once

#include <JuceHeader.h>

class Metronome final
{
public:
    void prepare(double newSampleRate);
    void setBpm(double newBpm);
    void setEnabled(bool shouldBeEnabled);
    bool isEnabled() const;
    void reset(double positionSeconds);
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         int startSample,
                         int numSamples,
                         double blockStartTimeSeconds);

private:
    void renderClick(juce::AudioBuffer<float>& buffer, int sampleOffset, bool downbeat);

    double sampleRate = 44100.0;
    double bpm = 120.0;
    double nextBeatTimeSeconds = 0.0;
    int beatIndex = 0;
    bool enabled = false;
};
