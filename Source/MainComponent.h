#pragma once

#include "AudioEngine.h"
#include "Midi/MidiInputManager.h"
#include "UI/TimelineComponent.h"

#include <JuceHeader.h>

#include <map>
#include <set>
#include <vector>

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
    void updateTitleDisplay();
    void updateSelectedTrackControls();
    void updateTransportDisplay();
    void updateButtonStates();
    void updateTimelineSize();
    void undoProjectEdit();
    void redoProjectEdit();
    void newProject();
    void goToStart();
    void goToEnd();
    void panicAllNotes();
    void renameSelectedTrack();
    void duplicateSelectedTrack();
    void clearAllTrackMuteSolo();
    void clearAllTrackArms();
    void toggleSelectedTrackArm();
    void armOnlySelectedTrack();
    void toggleSelectedTrackMute();
    void toggleSelectedTrackSolo();
    void soloOnlySelectedTrack();
    void resetSelectedTrackMix();
    void moveSelectedTrack(int direction);
    void selectAdjacentTrack(int direction);
    void fitProjectToView();
    void adjustMetronomeGain(float delta);
    void resetMasterVolume();
    void tapTempo();
    void cycleSnapGrid(int direction);
    void updateSnapButtonText();
    void addMarkerAtPlayhead();
    void renameMarkerNearPlayhead();
    void removeMarkerNearPlayhead();
    void jumpToMarker(int direction);
    void loopSelectedClip();
    void loopCurrentBar();
    void clearLoopRange();
    void movePlayheadByGrid(int direction, bool byBar);
    void importAudioToSelectedTrack();
    void nudgeSelectedClip(int direction);
    void trimSelectedClipEnd(int direction);
    void movePlayheadToSelectedClipBoundary(bool endBoundary);
    void moveSelectedClipToPlayhead();
    void selectAdjacentClip(int direction);
    void selectClipAtPlayhead();
    void alignSelectedClipToBar();
    void adjustSelectedAudioClipGain(float delta);
    void resetSelectedAudioClipGain();
    void normalizeSelectedAudioClip();
    void toggleSelectedAudioClipReverse();
    void duplicateSelectedClip();
    void duplicateSelectedClipAtPlayhead();
    void duplicateSelectedClipAtNextBar();
    void toggleSelectedClipMute();
    void deleteSelectedClip();
    void splitSelectedClip();
    void fadeInSelectedClip();
    void fadeOutSelectedClip();
    void adjustSelectedAudioClipFade(bool fadeIn, double deltaSeconds);
    void setSelectedAudioClipFades(double fadeSeconds);
    void clearSelectedAudioClipFades();
    void quantizeSelectedMidiClip();
    void swingQuantizeSelectedMidiClip();
    void quantizeSelectedMidiClipToScale(bool minorScale);
    void transposeSelectedMidiClip(int semitones);
    void invertSelectedMidiClip();
    void layerSelectedMidiClipOctave(int semitones);
    void adjustSelectedMidiClipVelocity(float delta);
    void setSelectedMidiClipVelocity(float velocity);
    void humanizeSelectedMidiClipVelocity();
    void humanizeSelectedMidiClipTiming();
    void legatoSelectedMidiClip();
    void staccatoSelectedMidiClip();
    void setSelectedMidiClipNoteLengthToGrid();
    void reverseSelectedMidiClip();
    void generateAiChordsForSelectedTrack();
    void generateAiBassForSelectedTrack();
    void generateAiArpForSelectedTrack();
    void exportMix();
    void exportSelectedTrack();
    void saveProject();
    void saveProjectAs();
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
    juce::Label peakLabel;
    juce::TextButton resetPeakButton;
    juce::Label midiInputLabel;
    juce::Label trackLabel;
    juce::Label zoomLabel;
    juce::TextButton newProjectButton;
    juce::TextButton openProjectButton;
    juce::TextButton saveProjectButton;
    juce::TextButton exportButton;
    juce::TextButton exportTrackButton;
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
    juce::TextButton aiBassButton;
    juce::TextButton aiArpButton;
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
    juce::TextButton monoButton;
    juce::Slider trackVolumeSlider;
    juce::Slider trackPanSlider;
    juce::ComboBox trackSelector;
    juce::ComboBox midiInputSelector;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File currentProjectFile;
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    std::set<int> activeComputerKeyboardNotes;
    std::vector<double> tapTempoTimes;
    int snapGridIndex = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
