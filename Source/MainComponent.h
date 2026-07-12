#pragma once

#include "AudioEngine.h"

#include <JuceHeader.h>

class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    // Draws the Step 2 application background.
    void paint(juce::Graphics& graphics) override;

    // Keeps the transport controls responsive during window resizing.
    void resized() override;

private:
    void openAudioFile();
    void updateButtonStates();
    void showErrorMessage(const juce::String& title, const juce::String& message);

    AudioEngine audioEngine;
    juce::AudioDeviceManager audioDeviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;

    juce::Label titleLabel;
    juce::Label fileNameLabel;
    juce::TextButton openButton;
    juce::TextButton playPauseButton;
    juce::TextButton stopButton;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
