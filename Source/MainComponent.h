#pragma once

#include "AudioEngine.h"
#include "Midi/MidiInputManager.h"
#include "UI/TimelineComponent.h"

#include <JuceHeader.h>

#include <map>
#include <set>

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
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

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
    void updateTimelineSize();
    void undoProjectEdit();
    void redoProjectEdit();
    void newProject();
    void panicAllNotes();
    void renameSelectedTrack();
    void moveSelectedTrack(int direction);
    void cycleSnapGrid(int direction);
    void updateSnapButtonText();
    void addMarkerAtPlayhead();
    void removeMarkerNearPlayhead();
    void jumpToMarker(int direction);
    void loopSelectedClip();
    void movePlayheadByGrid(int direction, bool byBar);
    void importAudioToSelectedTrack();
    void nudgeSelectedClip(int direction);
    void moveSelectedClipToPlayhead();
    void adjustSelectedAudioClipGain(float delta);
    void duplicateSelectedClip();
    void duplicateSelectedClipAtPlayhead();
    void deleteSelectedClip();
    void splitSelectedClip();
    void fadeInSelectedClip();
    void fadeOutSelectedClip();
    void quantizeSelectedMidiClip();
    void transposeSelectedMidiClip(int semitones);
    void adjustSelectedMidiClipVelocity(float delta);
    void generateAiChordsForSelectedTrack();
    void exportMix();
    void saveProject();
    void openProject();
    void showErrorMessage(const juce::String& title, const juce::String& message);
    void showInfoMessage(const juce::String& title, const juce::String& message);
    void selectTrackById(const TrackId& trackId);
    TrackSelection getSelectedTrack() const;
    std::optional<int> getComputerKeyboardNoteForKey(int keyCode) const;

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
    juce::Label zoomLabel;
    juce::TextButton newProjectButton;
    juce::TextButton openProjectButton;
    juce::TextButton saveProjectButton;
    juce::TextButton exportButton;
    juce::TextButton addAudioTrackButton;
    juce::TextButton addMidiTrackButton;
    juce::TextButton renameTrackButton;
    juce::TextButton duplicateTrackButton;
    juce::TextButton deleteTrackButton;
    juce::TextButton importAudioButton;
    juce::TextButton duplicateClipButton;
    juce::TextButton deleteClipButton;
    juce::TextButton splitClipButton;
    juce::TextButton fadeInButton;
    juce::TextButton fadeOutButton;
    juce::TextButton quantizeMidiButton;
    juce::TextButton snapButton;
    juce::TextButton aiChordsButton;
    juce::TextButton playPauseButton;
    juce::TextButton stopButton;
    juce::TextButton recordButton;
    juce::TextButton loopButton;
    juce::TextButton loopClipButton;
    juce::TextButton metronomeButton;
    juce::TextButton panicButton;
    juce::TextButton armButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::Slider bpmSlider;
    juce::Slider zoomSlider;
    juce::Slider masterVolumeSlider;
    juce::Slider trackVolumeSlider;
    juce::Slider trackPanSlider;
    juce::ComboBox trackSelector;
    juce::ComboBox midiInputSelector;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    std::set<int> activeComputerKeyboardNotes;
    int snapGridIndex = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
