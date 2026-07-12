#pragma once

#include <JuceHeader.h>

class TempoMap final
{
public:
    void setBpm(double newBpm);
    double getBpm() const;

    double secondsToBeats(double seconds) const;
    double beatsToSeconds(double beats) const;
    int getBarForBeats(double beats) const;
    int getBeatInBar(double beats) const;

    int getNumerator() const;
    int getDenominator() const;

private:
    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
};
