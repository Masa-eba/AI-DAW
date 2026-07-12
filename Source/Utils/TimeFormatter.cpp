#include "TimeFormatter.h"

#include <cmath>

juce::String TimeFormatter::formatSeconds(double seconds)
{
    if (! std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    const auto totalSeconds = static_cast<int>(seconds);
    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;

    return juce::String::formatted("%02d:%02d", minutes, remainingSeconds);
}
