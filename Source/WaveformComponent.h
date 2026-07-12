#pragma once

#include <JuceHeader.h>

#include <functional>

class WaveformComponent final : public juce::Component,
                                private juce::ChangeListener
{
public:
    WaveformComponent();
    ~WaveformComponent() override;

    // Loads a file into the thumbnail renderer.
    void setFile(const juce::File& file);

    // Clears the current thumbnail and playback position.
    void clear();

    // Updates the playhead position in seconds.
    void setCurrentPosition(double seconds);

    // Sets the audio length used for drawing and seeking.
    void setAudioLength(double seconds);

    void paint(juce::Graphics& graphics) override;
    void mouseDown(const juce::MouseEvent& event) override;

    std::function<void(double)> onSeek;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail;
    double audioLengthSeconds = 0.0;
    double currentPositionSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
