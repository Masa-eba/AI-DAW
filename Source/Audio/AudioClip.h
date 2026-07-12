#pragma once

#include <JuceHeader.h>

struct AudioClip
{
    juce::Uuid id;
    juce::File sourceFile;
    double startTimeSeconds = 0.0;
    double sourceOffsetSeconds = 0.0;
    double lengthSeconds = 0.0;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    float gain = 1.0f;
    bool muted = false;
};
