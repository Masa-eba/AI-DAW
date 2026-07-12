#pragma once

#include "AudioEngine.h"
#include "Midi/MidiInputManager.h"
#include "UI/TimelineComponent.h"

#include <JuceHeader.h>

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Draws the DTM application background.
    void paint(juce::Graphics& graphics) override;

    // Lays out transport, track controls, timeline, and keyboard.
    void resized() override;

private:
    struct TrackSelection
    {
        TrackId id;
        TrackType type = TrackType::Audio;
    };

    void timerCallback() override;
    void refreshMidiDevices();
    void refreshTrackSelector();
    void selectFirstTrackIfNeeded();
    void updateSelectedTrackControls();
    void updateTransportDisplay();
    void updateButtonStates();
    void importAudioToSelectedTrack();
    void exportMix();
    void saveProject();
    void openProject();
    void showErrorMessage(const juce::String& title, const juce::String& message);
    void showInfoMessage(const juce::String& title, const juce::String& message);
    TrackSelection getSelectedTrack() const;

    AudioEngine audioEngine;
    juce::AudioDeviceManager audioDeviceManager;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;
    MidiInputManager midiInputManager;

    TimelineComponent timelineComponent;
    juce::Viewport timelineViewport;

    juce::Label titleLabel;
    juce::Label positionLabel;
    juce::Label bpmLabel;
    juce::Label masterLabel;
    juce::Label midiInputLabel;
    juce::Label trackLabel;
    juce::TextButton openProjectButton;
    juce::TextButton saveProjectButton;
    juce::TextButton exportButton;
    juce::TextButton addAudioTrackButton;
    juce::TextButton addMidiTrackButton;
    juce::TextButton deleteTrackButton;
    juce::TextButton importAudioButton;
    juce::TextButton playPauseButton;
    juce::TextButton stopButton;
    juce::TextButton recordButton;
    juce::TextButton metronomeButton;
    juce::TextButton armButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::Slider bpmSlider;
    juce::Slider masterVolumeSlider;
    juce::Slider trackVolumeSlider;
    juce::ComboBox trackSelector;
    juce::ComboBox midiInputSelector;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Array<juce::MidiDeviceInfo> midiDevices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
