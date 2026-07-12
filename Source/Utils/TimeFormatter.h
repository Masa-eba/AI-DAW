#pragma once

#include <JuceHeader.h>

class TimeFormatter final
{
public:
    // Formats seconds as MM:SS, treating invalid values as zero.
    static juce::String formatSeconds(double seconds);
};
