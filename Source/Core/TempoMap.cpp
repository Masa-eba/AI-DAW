#include "TempoMap.h"

#include <cmath>

void TempoMap::setBpm(double newBpm)
{
    if (! std::isfinite(newBpm))
        newBpm = 120.0;

    bpm = juce::jlimit(20.0, 300.0, newBpm);
}

double TempoMap::getBpm() const
{
    return bpm;
}

double TempoMap::secondsToBeats(double seconds) const
{
    if (! std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    return seconds / (60.0 / bpm);
}

double TempoMap::beatsToSeconds(double beats) const
{
    if (! std::isfinite(beats) || beats < 0.0)
        beats = 0.0;

    return beats * (60.0 / bpm);
}

int TempoMap::getBarForBeats(double beats) const
{
    return static_cast<int>(std::floor(juce::jmax(0.0, beats) / numerator)) + 1;
}

int TempoMap::getBeatInBar(double beats) const
{
    return static_cast<int>(std::floor(juce::jmax(0.0, beats))) % numerator + 1;
}

int TempoMap::getNumerator() const
{
    return numerator;
}

int TempoMap::getDenominator() const
{
    return denominator;
}
