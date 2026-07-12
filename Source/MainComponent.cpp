#include "MainComponent.h"

#include "TimeFormatter.h"

#include <array>
#include <cmath>
#include <limits>

MainComponent::MainComponent()
    : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      midiInputManager(keyboardState)
{
    audioEngine.setMidiKeyboardState(&keyboardState);

    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    updateTitleDisplay();

    positionLabel.setText("00:00 / 00:00   Bar 1 Beat 1", juce::dontSendNotification);
    positionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(positionLabel);

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmLabel);

    masterLabel.setText("Master", juce::dontSendNotification);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(masterLabel);

    peakLabel.setText("Peak -inf dB", juce::dontSendNotification);
    peakLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(peakLabel);

    resetPeakButton.setButtonText("Reset Peak");
    resetPeakButton.onClick = [this] { audioEngine.resetHeldOutputPeak(); };
    addAndMakeVisible(resetPeakButton);

    midiInputLabel.setText("MIDI Input", juce::dontSendNotification);
    midiInputLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(midiInputLabel);

    trackLabel.setText("Track", juce::dontSendNotification);
    trackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(trackLabel);

    zoomLabel.setText("Zoom", juce::dontSendNotification);
    zoomLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(zoomLabel);

    newProjectButton.setButtonText("New");
    newProjectButton.onClick = [this] { newProject(); };
    addAndMakeVisible(newProjectButton);

    openProjectButton.setButtonText("Open Project");
    openProjectButton.onClick = [this] { openProject(); };
    addAndMakeVisible(openProjectButton);

    saveProjectButton.setButtonText("Save");
    saveProjectButton.onClick = [this] { saveProject(); };
    addAndMakeVisible(saveProjectButton);

    exportButton.setButtonText("Export Mix");
    exportButton.onClick = [this] { exportMix(); };
    addAndMakeVisible(exportButton);

    exportTrackButton.setButtonText("Export Track");
    exportTrackButton.onClick = [this] { exportSelectedTrack(); };
    addAndMakeVisible(exportTrackButton);

    addAudioTrackButton.setButtonText("+ Audio Track");
    addAudioTrackButton.onClick = [this]
    {
        const auto id = audioEngine.addAudioTrack();
        refreshTrackSelector();
        selectTrackById(id);
    };
    addAndMakeVisible(addAudioTrackButton);

    addMidiTrackButton.setButtonText("+ MIDI Track");
    addMidiTrackButton.onClick = [this]
    {
        const auto id = audioEngine.addMidiTrack();
        refreshTrackSelector();
        selectTrackById(id);
    };
    addAndMakeVisible(addMidiTrackButton);

    renameTrackButton.setButtonText("Rename");
    renameTrackButton.onClick = [this] { renameSelectedTrack(); };
    addAndMakeVisible(renameTrackButton);

    duplicateTrackButton.setButtonText("Dup Track");
    duplicateTrackButton.onClick = [this] { duplicateSelectedTrack(); };
    addAndMakeVisible(duplicateTrackButton);

    deleteTrackButton.setButtonText("Delete");
    deleteTrackButton.onClick = [this]
    {
        const auto selected = getSelectedTrack();
        audioEngine.removeTrack(selected.id);
        refreshTrackSelector();
    };
    addAndMakeVisible(deleteTrackButton);

    importAudioButton.setButtonText("Import Audio");
    importAudioButton.onClick = [this] { importAudioToSelectedTrack(); };
    addAndMakeVisible(importAudioButton);

    duplicateClipButton.setButtonText("Duplicate Clip");
    duplicateClipButton.onClick = [this] { duplicateSelectedClip(); };
    addAndMakeVisible(duplicateClipButton);

    deleteClipButton.setButtonText("Delete Clip");
    deleteClipButton.onClick = [this] { deleteSelectedClip(); };
    addAndMakeVisible(deleteClipButton);

    splitClipButton.setButtonText("Split");
    splitClipButton.onClick = [this] { splitSelectedClip(); };
    addAndMakeVisible(splitClipButton);

    fadeInButton.setButtonText("Fade In");
    fadeInButton.onClick = [this] { fadeInSelectedClip(); };
    addAndMakeVisible(fadeInButton);

    fadeOutButton.setButtonText("Fade Out");
    fadeOutButton.onClick = [this] { fadeOutSelectedClip(); };
    addAndMakeVisible(fadeOutButton);

    quantizeMidiButton.setButtonText("Quantize");
    quantizeMidiButton.onClick = [this] { quantizeSelectedMidiClip(); };
    addAndMakeVisible(quantizeMidiButton);

    snapButton.setClickingTogglesState(true);
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this]
    {
        timelineComponent.setSnapEnabled(snapButton.getToggleState());
        updateSnapButtonText();
    };
    updateSnapButtonText();
    addAndMakeVisible(snapButton);

    aiChordsButton.setButtonText("AI Chords");
    aiChordsButton.onClick = [this] { generateAiChordsForSelectedTrack(); };
    addAndMakeVisible(aiChordsButton);

    aiBassButton.setButtonText("AI Bass");
    aiBassButton.onClick = [this] { generateAiBassForSelectedTrack(); };
    addAndMakeVisible(aiBassButton);

    aiArpButton.setButtonText("AI Arp");
    aiArpButton.onClick = [this] { generateAiArpForSelectedTrack(); };
    addAndMakeVisible(aiArpButton);

    playPauseButton.setButtonText("Play");
    playPauseButton.onClick = [this]
    {
        if (audioEngine.isPlaying())
            audioEngine.pause();
        else
            audioEngine.play();

        updateButtonStates();
    };
    addAndMakeVisible(playPauseButton);

    stopButton.setButtonText("Stop");
    stopButton.onClick = [this]
    {
        audioEngine.stop();
        updateButtonStates();
    };
    addAndMakeVisible(stopButton);

    recordButton.setButtonText("Record");
    recordButton.onClick = [this]
    {
        if (audioEngine.isRecording())
        {
            audioEngine.stopRecording();
            audioEngine.pause();
        }
        else if (! audioEngine.startRecording())
        {
            showErrorMessage("Recording failed", "Arm an audio or MIDI track before recording.");
        }

        updateButtonStates();
        timelineComponent.repaint();
    };
    addAndMakeVisible(recordButton);

    loopButton.setButtonText("Loop");
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this]
    {
        audioEngine.setLoopEnabled(loopButton.getToggleState());
        if (! loopButton.getToggleState())
        {
            audioEngine.clearLoopRange();
            timelineComponent.clearLoopRange();
        }
    };
    addAndMakeVisible(loopButton);

    loopClipButton.setButtonText("Loop Clip");
    loopClipButton.onClick = [this] { loopSelectedClip(); };
    addAndMakeVisible(loopClipButton);

    metronomeButton.setButtonText("Metronome");
    metronomeButton.setClickingTogglesState(true);
    metronomeButton.onClick = [this]
    {
        audioEngine.setMetronomeEnabled(metronomeButton.getToggleState());
    };
    addAndMakeVisible(metronomeButton);

    panicButton.setButtonText("Panic");
    panicButton.onClick = [this] { panicAllNotes(); };
    addAndMakeVisible(panicButton);

    armButton.setButtonText("R");
    armButton.setClickingTogglesState(true);
    armButton.onClick = [this]
    {
        const auto selected = getSelectedTrack();
        audioEngine.setTrackArmed(selected.id, armButton.getToggleState());
        updateSelectedTrackControls();
    };
    addAndMakeVisible(armButton);

    muteButton.setButtonText("M");
    muteButton.setClickingTogglesState(true);
    muteButton.onClick = [this]
    {
        const auto selected = getSelectedTrack();
        audioEngine.setTrackMuted(selected.id, muteButton.getToggleState());
        timelineComponent.repaint();
    };
    addAndMakeVisible(muteButton);

    soloButton.setButtonText("S");
    soloButton.setClickingTogglesState(true);
    soloButton.onClick = [this]
    {
        const auto selected = getSelectedTrack();
        audioEngine.setTrackSolo(selected.id, soloButton.getToggleState());
        timelineComponent.repaint();
    };
    addAndMakeVisible(soloButton);

    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    bpmSlider.setRange(20.0, 300.0, 1.0);
    bpmSlider.setValue(120.0, juce::dontSendNotification);
    bpmSlider.onValueChange = [this]
    {
        audioEngine.setBpm(bpmSlider.getValue());
        updateTimelineSize();
        timelineComponent.repaint();
    };
    addAndMakeVisible(bpmSlider);

    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    zoomSlider.setRange(20.0, 500.0, 1.0);
    zoomSlider.setValue(100.0, juce::dontSendNotification);
    zoomSlider.onValueChange = [this]
    {
        timelineComponent.setPixelsPerSecond(zoomSlider.getValue());
        updateTimelineSize();
    };
    addAndMakeVisible(zoomSlider);

    masterVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    masterVolumeSlider.setRange(0.0, 1.0, 0.01);
    masterVolumeSlider.setValue(0.8, juce::dontSendNotification);
    masterVolumeSlider.onValueChange = [this]
    {
        audioEngine.setGain(static_cast<float>(masterVolumeSlider.getValue()));
    };
    addAndMakeVisible(masterVolumeSlider);

    monoButton.setButtonText("Mono");
    monoButton.setClickingTogglesState(true);
    monoButton.onClick = [this]
    {
        audioEngine.setMonoMonitoringEnabled(monoButton.getToggleState());
    };
    addAndMakeVisible(monoButton);

    trackVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trackVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    trackVolumeSlider.setRange(0.0, 1.0, 0.01);
    trackVolumeSlider.setValue(0.8, juce::dontSendNotification);
    trackVolumeSlider.onValueChange = [this]
    {
        const auto selected = getSelectedTrack();
        audioEngine.setTrackGain(selected.id, static_cast<float>(trackVolumeSlider.getValue()));
    };
    addAndMakeVisible(trackVolumeSlider);

    trackPanSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trackPanSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    trackPanSlider.setRange(-1.0, 1.0, 0.01);
    trackPanSlider.setValue(0.0, juce::dontSendNotification);
    trackPanSlider.onValueChange = [this]
    {
        const auto selected = getSelectedTrack();
        audioEngine.setTrackPan(selected.id, static_cast<float>(trackPanSlider.getValue()));
    };
    addAndMakeVisible(trackPanSlider);

    trackSelector.onChange = [this] { updateSelectedTrackControls(); };
    addAndMakeVisible(trackSelector);

    midiInputSelector.onChange = [this]
    {
        const auto index = midiInputSelector.getSelectedItemIndex();

        if (juce::isPositiveAndBelow(index, midiDevices.size()))
            if (! midiInputManager.openDevice(midiDevices.getReference(index)))
                showErrorMessage("MIDI device error", "The selected MIDI input could not be opened.");
    };
    addAndMakeVisible(midiInputSelector);

    timelineComponent.setProjectModel(&audioEngine.getProjectModel());
    timelineComponent.onSeek = [this](double seconds)
    {
        audioEngine.setPosition(seconds);
        updateTransportDisplay();
    };
    timelineComponent.onAudioClipMoved = [this](const TrackId& trackId,
                                                const juce::Uuid& clipId,
                                                double startTimeSeconds)
    {
        audioEngine.setAudioClipStartTime(trackId, clipId, startTimeSeconds);
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineComponent.onAudioClipTrimmed = [this](const TrackId& trackId,
                                                  const juce::Uuid& clipId,
                                                  double startTimeSeconds,
                                                  double sourceOffsetSeconds,
                                                  double lengthSeconds)
    {
        audioEngine.setAudioClipTiming(trackId,
                                       clipId,
                                       startTimeSeconds,
                                       sourceOffsetSeconds,
                                       lengthSeconds);
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineComponent.onAudioClipMovedToTrack = [this](const TrackId& sourceTrackId,
                                                       const TrackId& destinationTrackId,
                                                       const juce::Uuid& clipId,
                                                       double startTimeSeconds)
    {
        audioEngine.moveAudioClipToTrack(sourceTrackId, destinationTrackId, clipId, startTimeSeconds);
        refreshTrackSelector();
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineComponent.onMidiClipMoved = [this](const TrackId& trackId,
                                               const juce::Uuid& clipId,
                                               double startBeat)
    {
        audioEngine.setMidiClipStartBeat(trackId, clipId, startBeat);
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineComponent.onMidiClipTrimmed = [this](const TrackId& trackId,
                                                 const juce::Uuid& clipId,
                                                 double lengthBeats)
    {
        audioEngine.setMidiClipLength(trackId, clipId, lengthBeats);
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineComponent.onMidiClipMovedToTrack = [this](const TrackId& sourceTrackId,
                                                      const TrackId& destinationTrackId,
                                                      const juce::Uuid& clipId,
                                                      double startBeat)
    {
        audioEngine.moveMidiClipToTrack(sourceTrackId, destinationTrackId, clipId, startBeat);
        refreshTrackSelector();
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineComponent.onAudioFileDropped = [this](const TrackId& trackId,
                                                  const juce::File& file,
                                                  double startTimeSeconds)
    {
        if (! audioEngine.loadAudioFileToTrack(trackId, file))
        {
            showErrorMessage("Failed to open file",
                             "The dropped audio file could not be loaded.");
            return;
        }

        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(trackId);
            track != nullptr && ! track->clips.empty())
        {
            audioEngine.setAudioClipStartTime(trackId, track->clips.back().id, startTimeSeconds);
        }
        refreshTrackSelector();
        updateTimelineSize();
        timelineComponent.repaint();
        updateTransportDisplay();
    };
    timelineViewport.setViewedComponent(&timelineComponent, false);
    addAndMakeVisible(timelineViewport);

    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setAvailableRange(48, 72);
    keyboardComponent.setKeyWidth(24.0f);

    const auto audioDeviceError = audioDeviceManager.initialiseWithDefaultDevices(2, 2);

    if (audioDeviceError.isNotEmpty())
        showErrorMessage("Audio device error", audioDeviceError);
    else
        audioDeviceManager.addAudioCallback(&audioEngine);

    refreshTrackSelector();
    refreshMidiDevices();
    updateButtonStates();
    setWantsKeyboardFocus(true);
    startTimerHz(30);
    setSize(1200, 700);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.stop();
    audioDeviceManager.removeAudioCallback(&audioEngine);
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff202124));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    titleLabel.setBounds(area.removeFromTop(42));

    auto projectBar = area.removeFromTop(36);
    newProjectButton.setBounds(projectBar.removeFromLeft(72).reduced(0, 3));
    projectBar.removeFromLeft(8);
    openProjectButton.setBounds(projectBar.removeFromLeft(120).reduced(0, 3));
    projectBar.removeFromLeft(8);
    saveProjectButton.setBounds(projectBar.removeFromLeft(78).reduced(0, 3));
    projectBar.removeFromLeft(8);
    exportButton.setBounds(projectBar.removeFromLeft(110).reduced(0, 3));
    projectBar.removeFromLeft(8);
    exportTrackButton.setBounds(projectBar.removeFromLeft(118).reduced(0, 3));
    projectBar.removeFromLeft(16);
    addAudioTrackButton.setBounds(projectBar.removeFromLeft(130).reduced(0, 3));
    projectBar.removeFromLeft(8);
    addMidiTrackButton.setBounds(projectBar.removeFromLeft(124).reduced(0, 3));

    auto transportBar = area.removeFromTop(44);
    playPauseButton.setBounds(transportBar.removeFromLeft(88).reduced(0, 5));
    transportBar.removeFromLeft(8);
    stopButton.setBounds(transportBar.removeFromLeft(76).reduced(0, 5));
    transportBar.removeFromLeft(8);
    recordButton.setBounds(transportBar.removeFromLeft(92).reduced(0, 5));
    transportBar.removeFromLeft(12);
    loopButton.setBounds(transportBar.removeFromLeft(72).reduced(0, 5));
    transportBar.removeFromLeft(8);
    loopClipButton.setBounds(transportBar.removeFromLeft(92).reduced(0, 5));
    transportBar.removeFromLeft(8);
    bpmLabel.setBounds(transportBar.removeFromLeft(38));
    bpmSlider.setBounds(transportBar.removeFromLeft(160).reduced(0, 4));
    transportBar.removeFromLeft(8);
    metronomeButton.setBounds(transportBar.removeFromLeft(112).reduced(0, 5));
    transportBar.removeFromLeft(12);
    panicButton.setBounds(transportBar.removeFromLeft(72).reduced(0, 5));
    transportBar.removeFromLeft(12);
    positionLabel.setBounds(transportBar);

    auto trackBar = area.removeFromTop(44);
    trackLabel.setBounds(trackBar.removeFromLeft(48));
    trackSelector.setBounds(trackBar.removeFromLeft(180).reduced(0, 5));
    trackBar.removeFromLeft(8);
    importAudioButton.setBounds(trackBar.removeFromLeft(112).reduced(0, 5));
    trackBar.removeFromLeft(8);
    renameTrackButton.setBounds(trackBar.removeFromLeft(82).reduced(0, 5));
    trackBar.removeFromLeft(8);
    duplicateTrackButton.setBounds(trackBar.removeFromLeft(92).reduced(0, 5));
    trackBar.removeFromLeft(8);
    deleteTrackButton.setBounds(trackBar.removeFromLeft(80).reduced(0, 5));
    trackBar.removeFromLeft(8);
    armButton.setBounds(trackBar.removeFromLeft(36).reduced(0, 5));
    trackBar.removeFromLeft(4);
    muteButton.setBounds(trackBar.removeFromLeft(36).reduced(0, 5));
    trackBar.removeFromLeft(4);
    soloButton.setBounds(trackBar.removeFromLeft(36).reduced(0, 5));
    trackBar.removeFromLeft(8);
    trackVolumeSlider.setBounds(trackBar.removeFromLeft(190).reduced(0, 5));
    trackBar.removeFromLeft(8);
    trackPanSlider.setBounds(trackBar.removeFromLeft(170).reduced(0, 5));

    auto clipBar = area.removeFromTop(44);
    duplicateClipButton.setBounds(clipBar.removeFromLeft(118).reduced(0, 5));
    clipBar.removeFromLeft(8);
    deleteClipButton.setBounds(clipBar.removeFromLeft(92).reduced(0, 5));
    clipBar.removeFromLeft(8);
    splitClipButton.setBounds(clipBar.removeFromLeft(64).reduced(0, 5));
    clipBar.removeFromLeft(8);
    fadeInButton.setBounds(clipBar.removeFromLeft(76).reduced(0, 5));
    clipBar.removeFromLeft(8);
    fadeOutButton.setBounds(clipBar.removeFromLeft(84).reduced(0, 5));
    clipBar.removeFromLeft(8);
    quantizeMidiButton.setBounds(clipBar.removeFromLeft(84).reduced(0, 5));
    clipBar.removeFromLeft(8);
    snapButton.setBounds(clipBar.removeFromLeft(98).reduced(0, 5));
    clipBar.removeFromLeft(8);
    aiChordsButton.setBounds(clipBar.removeFromLeft(92).reduced(0, 5));
    clipBar.removeFromLeft(8);
    aiBassButton.setBounds(clipBar.removeFromLeft(78).reduced(0, 5));
    clipBar.removeFromLeft(8);
    aiArpButton.setBounds(clipBar.removeFromLeft(72).reduced(0, 5));

    area.removeFromTop(8);
    auto bottom = area.removeFromBottom(126);
    timelineViewport.setBounds(area);

    auto midiBar = bottom.removeFromTop(36);
    midiInputLabel.setBounds(midiBar.removeFromLeft(82));
    midiInputSelector.setBounds(midiBar.removeFromLeft(260).reduced(0, 4));
    midiBar.removeFromLeft(16);
    masterLabel.setBounds(midiBar.removeFromLeft(58));
    masterVolumeSlider.setBounds(midiBar.removeFromLeft(220).reduced(0, 4));
    midiBar.removeFromLeft(16);
    monoButton.setBounds(midiBar.removeFromLeft(64).reduced(0, 5));
    midiBar.removeFromLeft(16);
    peakLabel.setBounds(midiBar.removeFromLeft(110));
    midiBar.removeFromLeft(16);
    resetPeakButton.setBounds(midiBar.removeFromLeft(90).reduced(0, 5));
    midiBar.removeFromLeft(16);
    zoomLabel.setBounds(midiBar.removeFromLeft(48));
    zoomSlider.setBounds(midiBar.removeFromLeft(160).reduced(0, 4));

    keyboardComponent.setBounds(bottom.reduced(0, 8));
    updateTimelineSize();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'z')
    {
        if (key.getModifiers().isShiftDown())
            redoProjectEdit();
        else
            undoProjectEdit();

        return true;
    }

    if (key.getModifiers().isCommandDown()
        && ! key.getModifiers().isShiftDown()
        && ! key.getModifiers().isAltDown()
        && key.getKeyCode() == 'n')
    {
        newProject();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && ! key.getModifiers().isShiftDown()
        && ! key.getModifiers().isAltDown()
        && key.getKeyCode() == 'o')
    {
        openProject();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && ! key.getModifiers().isShiftDown()
        && ! key.getModifiers().isAltDown()
        && key.getKeyCode() == 's')
    {
        saveProject();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && ! key.getModifiers().isShiftDown()
        && ! key.getModifiers().isAltDown()
        && key.getKeyCode() == 'c')
    {
        copySelectedClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && ! key.getModifiers().isShiftDown()
        && ! key.getModifiers().isAltDown()
        && key.getKeyCode() == 'v')
    {
        pasteCopiedClipAtPlayhead();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 's')
    {
        saveProjectAs();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        panicAllNotes();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::homeKey)
    {
        goToStart();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::endKey)
    {
        goToEnd();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        selectAdjacentClip(key.getModifiers().isShiftDown() ? -1 : 1);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'd')
    {
        duplicateSelectedClipAtNextBar();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'd')
    {
        duplicateSelectedTrack();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'e')
    {
        exportSelectedTrack();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'e')
    {
        exportMix();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'd')
    {
        if (key.getModifiers().isShiftDown())
            duplicateSelectedClipAtPlayhead();
        else
            duplicateSelectedClip();

        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'e')
    {
        splitSelectedClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'k')
    {
        swingQuantizeSelectedMidiClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'g')
    {
        quantizeSelectedMidiClipToScale(true);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'u')
    {
        generateAiDrumsForSelectedTrack();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'y')
    {
        generateAiMelodyForSelectedTrack();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'p')
    {
        repeatSelectedClipToLoopEnd();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && ! key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'p')
    {
        duplicateSelectedClipAfterItself();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'g')
    {
        quantizeSelectedMidiClipToScale(false);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'k')
    {
        quantizeSelectedMidiClip();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'j')
    {
        moveSelectedClipToPlayhead();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'f')
    {
        clearSelectedAudioClipFades();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'b')
    {
        setSelectedAudioClipFades(0.05);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'i')
    {
        invertSelectedMidiClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'i')
    {
        adjustSelectedAudioClipFade(true, key.getModifiers().isShiftDown() ? -0.25 : 0.25);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'o')
    {
        adjustSelectedAudioClipFade(false, key.getModifiers().isShiftDown() ? -0.25 : 0.25);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'a')
    {
        clearAllTrackArms();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'a')
    {
        accentSelectedMidiClipVelocity();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'm')
    {
        addMarkerAtSelectedClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'u')
    {
        clearAllTrackMuteSolo();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'm')
    {
        toggleSelectedClipMute();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'n')
    {
        normalizeSelectedAudioClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'r')
    {
        reverseSelectedMidiClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'r')
    {
        toggleSelectedAudioClipReverse();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'h')
    {
        humanizeSelectedMidiClipVelocity();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'h')
    {
        humanizeSelectedMidiClipTiming();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'l')
    {
        setSelectedMidiClipNoteLengthToGrid();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'l')
    {
        loopBarsFromPlayhead(8);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'l')
    {
        staccatoSelectedMidiClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'l')
    {
        legatoSelectedMidiClip();
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::upKey)
    {
        transposeSelectedMidiClip(1);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::downKey)
    {
        transposeSelectedMidiClip(-1);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == juce::KeyPress::upKey)
    {
        layerSelectedMidiClipOctave(12);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == juce::KeyPress::downKey)
    {
        layerSelectedMidiClipOctave(-12);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == juce::KeyPress::upKey)
    {
        transposeSelectedMidiClip(12);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == juce::KeyPress::downKey)
    {
        transposeSelectedMidiClip(-12);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::leftKey)
    {
        trimSelectedClipEnd(-1);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::rightKey)
    {
        trimSelectedClipEnd(1);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == juce::KeyPress::leftKey)
    {
        movePlayheadToSelectedClipBoundary(false);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == juce::KeyPress::rightKey)
    {
        movePlayheadToSelectedClipBoundary(true);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == juce::KeyPress::leftKey)
    {
        nudgeSelectedClip(-1);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == juce::KeyPress::rightKey)
    {
        nudgeSelectedClip(1);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == ']')
    {
        adjustSelectedMidiClipVelocity(0.1f);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == '[')
    {
        adjustSelectedMidiClipVelocity(-0.1f);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getKeyCode() == 'v')
    {
        setSelectedMidiClipVelocity(0.8f);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == '=')
    {
        adjustSelectedAudioClipGain(0.1f);
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == '-')
    {
        adjustSelectedAudioClipGain(-0.1f);
        return true;
    }

    if (key.getModifiers().isCommandDown()
        && key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == '0')
    {
        resetSelectedAudioClipSourceStart();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == '0')
    {
        resetSelectedAudioClipGain();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == juce::KeyPress::leftKey)
    {
        movePlayheadByGrid(-1, key.getModifiers().isShiftDown());
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == juce::KeyPress::rightKey)
    {
        movePlayheadByGrid(1, key.getModifiers().isShiftDown());
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'c')
    {
        selectClipAtPlayhead();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'b')
    {
        clearLoopRange();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'b')
    {
        loopCurrentBar();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'q')
    {
        alignSelectedClipToBar();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == ']')
    {
        cycleSnapGrid(1);
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == '[')
    {
        cycleSnapGrid(-1);
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'm')
    {
        renameMarkerNearPlayhead();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'm')
    {
        addMarkerAtPlayhead();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == ',')
    {
        jumpToMarker(-1);
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == '.')
    {
        jumpToMarker(1);
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 'r')
    {
        armOnlySelectedTrack();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'r')
    {
        toggleSelectedTrackArm();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'm')
    {
        toggleSelectedTrackMute();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == 's')
    {
        soloOnlySelectedTrack();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 's')
    {
        toggleSelectedTrackSolo();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == '0')
    {
        resetMasterVolume();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == '0')
    {
        resetSelectedTrackMix();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 'f')
    {
        fitProjectToView();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == 't')
    {
        tapTempo();
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == '=')
    {
        adjustMetronomeGain(0.1f);
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == '-')
    {
        adjustMetronomeGain(-0.1f);
        return true;
    }

    if (key.getModifiers().isAltDown()
        && (key.getKeyCode() == juce::KeyPress::deleteKey
            || key.getKeyCode() == juce::KeyPress::backspaceKey))
    {
        removeMarkerNearPlayhead();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::upKey)
    {
        moveSelectedTrack(-1);
        return true;
    }

    if (key.getModifiers().isAltDown()
        && key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::downKey)
    {
        moveSelectedTrack(1);
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == juce::KeyPress::upKey)
    {
        selectAdjacentTrack(-1);
        return true;
    }

    if (key.getModifiers().isAltDown() && key.getKeyCode() == juce::KeyPress::downKey)
    {
        selectAdjacentTrack(1);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey
        || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        deleteSelectedClip();
        return true;
    }

    if (const auto note = getComputerKeyboardNoteForKey(key.getKeyCode()))
    {
        if (! activeComputerKeyboardNotes.contains(*note))
        {
            activeComputerKeyboardNotes.insert(*note);
            keyboardState.noteOn(1, *note, 0.9f);
        }

        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        if (audioEngine.isPlaying())
            audioEngine.pause();
        else
            audioEngine.play();

        updateButtonStates();
        return true;
    }

    return false;
}

bool MainComponent::keyStateChanged(bool isKeyDown)
{
    if (isKeyDown)
        return false;

    for (auto it = activeComputerKeyboardNotes.begin(); it != activeComputerKeyboardNotes.end();)
    {
        const auto note = *it;
        auto stillDown = false;

        for (const auto keyCode : { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k' })
            if (getComputerKeyboardNoteForKey(keyCode).value_or(-1) == note
                && juce::KeyPress::isKeyCurrentlyDown(keyCode))
                stillDown = true;

        if (stillDown)
        {
            ++it;
            continue;
        }

        keyboardState.noteOff(1, note, 0.0f);
        it = activeComputerKeyboardNotes.erase(it);
    }

    return true;
}

void MainComponent::timerCallback()
{
    updateTransportDisplay();
    updateButtonStates();
    timelineComponent.setPosition(audioEngine.getPosition());
}

void MainComponent::refreshMidiDevices()
{
    midiInputSelector.clear();
    midiDevices = midiInputManager.refreshDevices();

    for (auto i = 0; i < midiDevices.size(); ++i)
        midiInputSelector.addItem(midiDevices.getReference(i).name, i + 1);
}

void MainComponent::refreshTrackSelector()
{
    const auto previousId = trackSelector.getSelectedId();
    trackSelector.clear();
    auto itemId = 1;

    for (const auto& track : audioEngine.getProjectModel().getAudioTracks())
        trackSelector.addItem(track->state.name, itemId++);

    for (const auto& track : audioEngine.getProjectModel().getMidiTracks())
        trackSelector.addItem(track->state.name, itemId++);

    if (previousId > 0 && previousId < itemId)
        trackSelector.setSelectedId(previousId, juce::dontSendNotification);
    else
        selectFirstTrackIfNeeded();

    updateSelectedTrackControls();
    updateTimelineSize();
    timelineComponent.repaint();
}

void MainComponent::selectFirstTrackIfNeeded()
{
    if (trackSelector.getNumItems() > 0)
        trackSelector.setSelectedItemIndex(0, juce::dontSendNotification);
}

void MainComponent::updateTitleDisplay()
{
    auto title = juce::String("AI-DAW");

    if (currentProjectFile != juce::File{})
        title += " - " + currentProjectFile.getFileNameWithoutExtension();
    else
        title += " - Untitled";

    titleLabel.setText(title, juce::dontSendNotification);
}

void MainComponent::updateSelectedTrackControls()
{
    const auto selected = getSelectedTrack();
    const auto& model = audioEngine.getProjectModel();

    if (const auto* track = model.findAudioTrack(selected.id))
    {
        armButton.setToggleState(track->state.armed, juce::dontSendNotification);
        muteButton.setToggleState(track->state.muted, juce::dontSendNotification);
        soloButton.setToggleState(track->state.solo, juce::dontSendNotification);
        trackVolumeSlider.setValue(track->state.gain, juce::dontSendNotification);
        trackPanSlider.setValue(track->state.pan, juce::dontSendNotification);
        importAudioButton.setEnabled(true);
        return;
    }

    if (const auto* track = model.findMidiTrack(selected.id))
    {
        armButton.setToggleState(track->state.armed, juce::dontSendNotification);
        muteButton.setToggleState(track->state.muted, juce::dontSendNotification);
        soloButton.setToggleState(track->state.solo, juce::dontSendNotification);
        trackVolumeSlider.setValue(track->state.gain, juce::dontSendNotification);
        trackPanSlider.setValue(track->state.pan, juce::dontSendNotification);
        importAudioButton.setEnabled(false);
    }
}

void MainComponent::updateTransportDisplay()
{
    const auto position = audioEngine.getPosition();
    const auto length = audioEngine.getLength();
    const auto beats = audioEngine.getProjectModel().getTempoMap().secondsToBeats(position);
    const auto bar = audioEngine.getProjectModel().getTempoMap().getBarForBeats(beats);
    const auto beat = audioEngine.getProjectModel().getTempoMap().getBeatInBar(beats);

    positionLabel.setText(TimeFormatter::formatSeconds(position)
                              + " / "
                              + TimeFormatter::formatSeconds(length)
                              + "   Bar "
                              + juce::String(bar)
                              + " Beat "
                              + juce::String(beat),
                          juce::dontSendNotification);

    const auto heldPeak = audioEngine.getHeldOutputPeak();
    const auto peakText = heldPeak > 0.000001f
        ? juce::String(juce::Decibels::gainToDecibels(heldPeak), 1) + " dB"
        : "-inf dB";
    peakLabel.setText("Peak " + peakText, juce::dontSendNotification);
    peakLabel.setColour(juce::Label::textColourId,
                        heldPeak >= 1.0f ? juce::Colours::red : juce::Colours::lightgrey);
}

void MainComponent::updateButtonStates()
{
    playPauseButton.setButtonText(audioEngine.isPlaying() ? "Pause" : "Play");
    recordButton.setButtonText(audioEngine.isRecording() ? "Stop Rec" : "Record");
}

void MainComponent::updateTimelineSize()
{
    const auto timelineBounds = timelineViewport.getBounds();
    const auto visibleWidth = juce::jmax(1, timelineBounds.getWidth());
    const auto visibleHeight = juce::jmax(1, timelineBounds.getHeight());
    const auto contentWidth = static_cast<int>(audioEngine.getLength() * zoomSlider.getValue()) + 420;
    const auto trackCount = audioEngine.getProjectModel().getAudioTracks().size()
                          + audioEngine.getProjectModel().getMidiTracks().size();
    const auto contentHeight = 30 + static_cast<int>(trackCount) * 72 + 80;

    timelineComponent.setSize(juce::jmax(visibleWidth, contentWidth),
                              juce::jmax(visibleHeight, contentHeight));
}

void MainComponent::undoProjectEdit()
{
    if (! audioEngine.undo())
        return;

    refreshTrackSelector();
    updateTimelineSize();
    updateTransportDisplay();
    timelineComponent.repaint();
}

void MainComponent::redoProjectEdit()
{
    if (! audioEngine.redo())
        return;

    refreshTrackSelector();
    updateTimelineSize();
    updateTransportDisplay();
    timelineComponent.repaint();
}

void MainComponent::newProject()
{
    auto options = juce::MessageBoxOptions()
                       .withIconType(juce::MessageBoxIconType::WarningIcon)
                       .withTitle("New project")
                       .withMessage("Clear the current project and start a new one?")
                       .withButton("New Project")
                       .withButton("Cancel");
    juce::Component::SafePointer<MainComponent> safeThis(this);

    juce::AlertWindow::showAsync(options, [safeThis](int result)
    {
        if (safeThis == nullptr || result != 1)
            return;

        auto* component = safeThis.getComponent();
        component->audioEngine.newProject();
        component->currentProjectFile = juce::File();
        component->updateTitleDisplay();
        component->loopButton.setToggleState(false, juce::dontSendNotification);
        component->timelineComponent.clearLoopRange();
        component->refreshTrackSelector();
        component->updateTimelineSize();
        component->updateTransportDisplay();
        component->timelineComponent.repaint();
    });
}

void MainComponent::goToStart()
{
    audioEngine.setPosition(0.0);
    updateTransportDisplay();
    timelineComponent.setPosition(audioEngine.getPosition());
}

void MainComponent::goToEnd()
{
    audioEngine.setPosition(audioEngine.getLength());
    updateTransportDisplay();
    timelineComponent.setPosition(audioEngine.getPosition());
}

void MainComponent::panicAllNotes()
{
    for (const auto note : activeComputerKeyboardNotes)
        keyboardState.noteOff(1, note, 0.0f);

    activeComputerKeyboardNotes.clear();
    audioEngine.panic();
}

void MainComponent::moveSelectedTrack(int direction)
{
    const auto selected = getSelectedTrack();

    if (! audioEngine.moveTrack(selected.id, direction))
        return;

    refreshTrackSelector();
    selectTrackById(selected.id);
    updateTimelineSize();
    timelineComponent.repaint();
}

void MainComponent::selectAdjacentTrack(int direction)
{
    const auto numItems = trackSelector.getNumItems();

    if (numItems <= 0 || direction == 0)
        return;

    const auto currentIndex = juce::jlimit(0, numItems - 1, trackSelector.getSelectedItemIndex());
    const auto nextIndex = juce::jlimit(0, numItems - 1, currentIndex + (direction < 0 ? -1 : 1));

    if (nextIndex == currentIndex)
        return;

    trackSelector.setSelectedItemIndex(nextIndex, juce::sendNotificationSync);
    timelineComponent.repaint();
}

void MainComponent::fitProjectToView()
{
    const auto length = audioEngine.getLength();

    if (length <= 0.0)
        return;

    constexpr auto timelineLabelAndPadding = 190.0;
    const auto visibleWidth = juce::jmax(1.0, static_cast<double>(timelineViewport.getWidth()) - timelineLabelAndPadding);
    const auto fittedZoom = juce::jlimit(20.0, 500.0, visibleWidth / length);

    zoomSlider.setValue(fittedZoom, juce::sendNotificationSync);
    updateTimelineSize();
    timelineComponent.repaint();
}

void MainComponent::adjustMetronomeGain(float delta)
{
    const auto gain = juce::jlimit(0.0f, 1.0f, audioEngine.getMetronomeGain() + delta);
    audioEngine.setMetronomeGain(gain);
    metronomeButton.setButtonText("Metro " + juce::String(static_cast<int>(std::round(gain * 100.0f))) + "%");
}

void MainComponent::resetMasterVolume()
{
    masterVolumeSlider.setValue(0.8, juce::sendNotificationSync);
}

void MainComponent::tapTempo()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if (! tapTempoTimes.empty() && now - tapTempoTimes.back() > 2.0)
        tapTempoTimes.clear();

    tapTempoTimes.push_back(now);

    while (tapTempoTimes.size() > 5)
        tapTempoTimes.erase(tapTempoTimes.begin());

    if (tapTempoTimes.size() < 2)
        return;

    auto intervalSum = 0.0;
    auto intervalCount = 0;

    for (auto index = size_t { 1 }; index < tapTempoTimes.size(); ++index)
    {
        const auto interval = tapTempoTimes[index] - tapTempoTimes[index - 1];

        if (interval <= 0.0 || interval > 2.0)
            continue;

        intervalSum += interval;
        ++intervalCount;
    }

    if (intervalCount <= 0)
        return;

    const auto averageInterval = intervalSum / static_cast<double>(intervalCount);
    const auto bpm = juce::jlimit(20.0, 300.0, 60.0 / averageInterval);
    bpmSlider.setValue(std::round(bpm), juce::sendNotificationSync);
}

void MainComponent::cycleSnapGrid(int direction)
{
    static constexpr std::array<double, 4> gridValues { 1.0, 0.5, 0.25, 0.125 };

    snapGridIndex = juce::jlimit(0,
                                 static_cast<int>(gridValues.size()) - 1,
                                 snapGridIndex + direction);
    timelineComponent.setSnapGridBeats(gridValues[static_cast<size_t>(snapGridIndex)]);
    updateSnapButtonText();
}

void MainComponent::updateSnapButtonText()
{
    const auto gridBeats = timelineComponent.getSnapGridBeats();
    juce::String label = "Snap ";

    if (gridBeats >= 1.0)
        label += "1/4";
    else if (gridBeats >= 0.5)
        label += "1/8";
    else if (gridBeats >= 0.25)
        label += "1/16";
    else
        label += "1/32";

    if (! snapButton.getToggleState())
        label += " Off";

    snapButton.setButtonText(label);
}

void MainComponent::addMarkerAtPlayhead()
{
    audioEngine.addMarker(audioEngine.getPosition());
    updateTimelineSize();
    timelineComponent.repaint();
}

void MainComponent::addMarkerAtSelectedClip()
{
    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedAudioClip->second)
                    continue;

                const auto markerId = audioEngine.addMarker(clip.startTimeSeconds);
                audioEngine.renameMarker(markerId, clip.sourceFile.getFileNameWithoutExtension());
                updateTimelineSize();
                timelineComponent.repaint();
                return;
            }
        }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedMidiClip->second)
                    continue;

                const auto startSeconds = audioEngine.getProjectModel().getTempoMap().beatsToSeconds(clip.startBeat);
                const auto markerId = audioEngine.addMarker(startSeconds);
                audioEngine.renameMarker(markerId, track->state.name + " MIDI Clip");
                updateTimelineSize();
                timelineComponent.repaint();
                return;
            }
        }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before adding a marker.");
}

void MainComponent::renameMarkerNearPlayhead()
{
    const auto thresholdSeconds = audioEngine.getProjectModel().getTempoMap()
                                      .beatsToSeconds(timelineComponent.getSnapGridBeats()) * 0.5;
    const auto currentPosition = audioEngine.getPosition();
    const ProjectMarker* nearestMarker = nullptr;
    auto nearestDistance = std::numeric_limits<double>::max();

    for (const auto& marker : audioEngine.getProjectModel().getMarkers())
    {
        const auto distance = std::abs(marker.timeSeconds - currentPosition);

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestMarker = &marker;
        }
    }

    if (nearestMarker == nullptr || nearestDistance > thresholdSeconds)
    {
        showErrorMessage("No marker nearby", "Move the playhead close to a marker before renaming it.");
        return;
    }

    auto* window = new juce::AlertWindow("Rename marker",
                                         "Enter a new marker name.",
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("markerName", nearestMarker->name, "Marker name:");
    window->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    const auto markerId = nearestMarker->id;
    juce::Component::SafePointer<MainComponent> safeThis(this);
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create([safeThis, markerId, window](int result)
                            {
                                std::unique_ptr<juce::AlertWindow> ownedWindow(window);

                                if (safeThis == nullptr || result != 1)
                                    return;

                                auto* component = safeThis.getComponent();
                                const auto name = ownedWindow->getTextEditorContents("markerName");

                                if (! component->audioEngine.renameMarker(markerId, name))
                                {
                                    component->showErrorMessage("Rename failed", "The marker could not be renamed.");
                                    return;
                                }

                                component->timelineComponent.repaint();
                            }),
                            false);
}

void MainComponent::removeMarkerNearPlayhead()
{
    const auto thresholdSeconds = audioEngine.getProjectModel().getTempoMap()
                                      .beatsToSeconds(timelineComponent.getSnapGridBeats()) * 0.5;

    if (! audioEngine.removeNearestMarker(audioEngine.getPosition(), thresholdSeconds))
        return;

    updateTimelineSize();
    timelineComponent.repaint();
}

void MainComponent::jumpToMarker(int direction)
{
    const auto& markers = audioEngine.getProjectModel().getMarkers();

    if (markers.empty())
        return;

    const auto currentPosition = audioEngine.getPosition();
    const auto epsilon = 0.01;
    std::optional<double> target;

    if (direction > 0)
    {
        for (const auto& marker : markers)
        {
            if (marker.timeSeconds > currentPosition + epsilon)
            {
                target = marker.timeSeconds;
                break;
            }
        }

        if (! target.has_value())
            target = markers.front().timeSeconds;
    }
    else
    {
        for (auto iterator = markers.rbegin(); iterator != markers.rend(); ++iterator)
        {
            if (iterator->timeSeconds < currentPosition - epsilon)
            {
                target = iterator->timeSeconds;
                break;
            }
        }

        if (! target.has_value())
            target = markers.back().timeSeconds;
    }

    audioEngine.setPosition(*target);
    updateTransportDisplay();
    timelineComponent.setPosition(audioEngine.getPosition());
}

void MainComponent::movePlayheadByGrid(int direction, bool byBar)
{
    if (direction == 0)
        return;

    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto currentBeats = tempo.secondsToBeats(audioEngine.getPosition());
    const auto stepBeats = byBar ? static_cast<double>(tempo.getNumerator()) : 1.0;
    const auto epsilon = 1.0e-6;
    double targetBeats = 0.0;

    if (direction > 0)
        targetBeats = (std::floor(currentBeats / stepBeats + epsilon) + 1.0) * stepBeats;
    else
        targetBeats = juce::jmax(0.0, (std::ceil(currentBeats / stepBeats - epsilon) - 1.0) * stepBeats);

    audioEngine.setPosition(tempo.beatsToSeconds(targetBeats));
    updateTransportDisplay();
    timelineComponent.setPosition(audioEngine.getPosition());
}

void MainComponent::renameSelectedTrack()
{
    const auto selected = getSelectedTrack();
    juce::String currentName;

    if (const auto* audioTrack = audioEngine.getProjectModel().findAudioTrack(selected.id))
        currentName = audioTrack->state.name;
    else if (const auto* midiTrack = audioEngine.getProjectModel().findMidiTrack(selected.id))
        currentName = midiTrack->state.name;

    if (currentName.isEmpty())
    {
        showErrorMessage("No track selected", "Select a track before renaming.");
        return;
    }

    auto* window = new juce::AlertWindow("Rename track",
                                         "Enter a new track name.",
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("trackName", currentName, "Track name:");
    window->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<MainComponent> safeThis(this);
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create([safeThis, selected, window](int result)
                            {
                                std::unique_ptr<juce::AlertWindow> ownedWindow(window);

                                if (safeThis == nullptr || result != 1)
                                    return;

                                auto* component = safeThis.getComponent();
                                const auto name = ownedWindow->getTextEditorContents("trackName");

                                if (! component->audioEngine.renameTrack(selected.id, name))
                                {
                                    component->showErrorMessage("Rename failed", "The selected track could not be renamed.");
                                    return;
                                }

                                component->refreshTrackSelector();
                                component->timelineComponent.repaint();
                            }),
                            false);
}

void MainComponent::duplicateSelectedTrack()
{
    const auto selected = getSelectedTrack();
    const auto duplicatedId = audioEngine.duplicateTrack(selected.id);

    if (duplicatedId.isNull())
    {
        showErrorMessage("Duplicate failed", "The selected track could not be duplicated.");
        return;
    }

    refreshTrackSelector();
    selectTrackById(duplicatedId);
    updateTimelineSize();
    timelineComponent.repaint();
}

void MainComponent::clearAllTrackMuteSolo()
{
    audioEngine.clearTrackMuteSolo();
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::clearAllTrackArms()
{
    audioEngine.clearTrackArms();
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::toggleSelectedTrackArm()
{
    const auto selected = getSelectedTrack();
    auto armed = false;

    if (selected.type == TrackType::Audio)
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selected.id))
            armed = track->state.armed;
    }
    else if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selected.id))
    {
        armed = track->state.armed;
    }

    audioEngine.setTrackArmed(selected.id, ! armed);
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::armOnlySelectedTrack()
{
    const auto selected = getSelectedTrack();
    audioEngine.armOnlyTrack(selected.id);
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::toggleSelectedTrackMute()
{
    const auto selected = getSelectedTrack();
    auto muted = false;

    if (selected.type == TrackType::Audio)
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selected.id))
            muted = track->state.muted;
    }
    else if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selected.id))
    {
        muted = track->state.muted;
    }

    audioEngine.setTrackMuted(selected.id, ! muted);
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::toggleSelectedTrackSolo()
{
    const auto selected = getSelectedTrack();
    auto solo = false;

    if (selected.type == TrackType::Audio)
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selected.id))
            solo = track->state.solo;
    }
    else if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selected.id))
    {
        solo = track->state.solo;
    }

    audioEngine.setTrackSolo(selected.id, ! solo);
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::soloOnlySelectedTrack()
{
    const auto selected = getSelectedTrack();
    audioEngine.soloOnlyTrack(selected.id);
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::resetSelectedTrackMix()
{
    const auto selected = getSelectedTrack();
    audioEngine.setTrackGain(selected.id, 0.8f);
    audioEngine.setTrackPan(selected.id, 0.0f);
    updateSelectedTrackControls();
    timelineComponent.repaint();
}

void MainComponent::loopSelectedClip()
{
    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedAudioClip->second)
                {
                    audioEngine.setLoopRange(clip.startTimeSeconds,
                                             clip.startTimeSeconds + clip.lengthSeconds);
                    timelineComponent.setLoopRange(clip.startTimeSeconds,
                                                   clip.startTimeSeconds + clip.lengthSeconds);
                    audioEngine.setPosition(clip.startTimeSeconds);
                    loopButton.setToggleState(true, juce::dontSendNotification);
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedMidiClip->second)
                {
                    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
                    const auto startSeconds = tempo.beatsToSeconds(clip.startBeat);
                    const auto endSeconds = tempo.beatsToSeconds(clip.startBeat + clip.lengthBeats);
                    audioEngine.setLoopRange(startSeconds, endSeconds);
                    timelineComponent.setLoopRange(startSeconds, endSeconds);
                    audioEngine.setPosition(startSeconds);
                    loopButton.setToggleState(true, juce::dontSendNotification);
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before setting a clip loop.");
}

void MainComponent::loopCurrentBar()
{
    loopBarsFromPlayhead(1);
}

void MainComponent::loopBarsFromPlayhead(int numberOfBars)
{
    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto currentBeats = tempo.secondsToBeats(audioEngine.getPosition());
    const auto beatsPerBar = juce::jmax(1, tempo.getNumerator());
    const auto barIndex = static_cast<int>(std::floor(currentBeats / static_cast<double>(beatsPerBar)));
    const auto startBeat = static_cast<double>(barIndex * beatsPerBar);
    const auto safeBars = juce::jlimit(1, 64, numberOfBars);
    const auto endBeat = startBeat + static_cast<double>(beatsPerBar * safeBars);
    const auto startSeconds = tempo.beatsToSeconds(startBeat);
    const auto endSeconds = tempo.beatsToSeconds(endBeat);

    audioEngine.setLoopRange(startSeconds, endSeconds);
    timelineComponent.setLoopRange(startSeconds, endSeconds);
    audioEngine.setPosition(startSeconds);
    loopButton.setToggleState(true, juce::dontSendNotification);
    updateTransportDisplay();
    timelineComponent.repaint();
}

void MainComponent::clearLoopRange()
{
    audioEngine.setLoopEnabled(false);
    audioEngine.clearLoopRange();
    timelineComponent.clearLoopRange();
    loopButton.setToggleState(false, juce::dontSendNotification);
    timelineComponent.repaint();
}

void MainComponent::nudgeSelectedClip(int direction)
{
    if (direction == 0)
        return;

    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto stepBeats = timelineComponent.getSnapGridBeats();
    const auto stepSeconds = tempo.beatsToSeconds(stepBeats);

    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedAudioClip->second)
                {
                    audioEngine.setAudioClipStartTime(selectedAudioClip->first,
                                                      selectedAudioClip->second,
                                                      juce::jmax(0.0, clip.startTimeSeconds
                                                                  + static_cast<double>(direction) * stepSeconds));
                    updateTimelineSize();
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedMidiClip->second)
                {
                    audioEngine.setMidiClipStartBeat(selectedMidiClip->first,
                                                     selectedMidiClip->second,
                                                     juce::jmax(0.0, clip.startBeat
                                                                 + static_cast<double>(direction) * stepBeats));
                    updateTimelineSize();
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before nudging.");
}

void MainComponent::trimSelectedClipEnd(int direction)
{
    if (direction == 0)
        return;

    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto stepBeats = timelineComponent.getSnapGridBeats();
    const auto stepSeconds = tempo.beatsToSeconds(stepBeats);

    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedAudioClip->second)
                {
                    if (! audioEngine.setAudioClipTiming(selectedAudioClip->first,
                                                         selectedAudioClip->second,
                                                         clip.startTimeSeconds,
                                                         clip.sourceOffsetSeconds,
                                                         clip.lengthSeconds
                                                             + static_cast<double>(direction) * stepSeconds))
                    {
                        showErrorMessage("Trim failed", "The selected audio clip could not be trimmed further.");
                        return;
                    }

                    updateTimelineSize();
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedMidiClip->second)
                {
                    if (! audioEngine.setMidiClipLength(selectedMidiClip->first,
                                                        selectedMidiClip->second,
                                                        clip.lengthBeats
                                                            + static_cast<double>(direction) * stepBeats))
                    {
                        showErrorMessage("Trim failed", "The selected MIDI clip could not be trimmed further.");
                        return;
                    }

                    updateTimelineSize();
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before trimming.");
}

void MainComponent::movePlayheadToSelectedClipBoundary(bool endBoundary)
{
    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedAudioClip->second)
                {
                    audioEngine.setPosition(endBoundary
                        ? clip.startTimeSeconds + clip.lengthSeconds
                        : clip.startTimeSeconds);
                    updateTransportDisplay();
                    timelineComponent.setPosition(audioEngine.getPosition());
                    return;
                }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        const auto& tempo = audioEngine.getProjectModel().getTempoMap();

        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
            for (const auto& clip : track->clips)
                if (clip.id == selectedMidiClip->second)
                {
                    audioEngine.setPosition(tempo.beatsToSeconds(endBoundary
                        ? clip.startBeat + clip.lengthBeats
                        : clip.startBeat));
                    updateTransportDisplay();
                    timelineComponent.setPosition(audioEngine.getPosition());
                    return;
                }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before moving the playhead.");
}

void MainComponent::moveSelectedClipToPlayhead()
{
    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        audioEngine.setAudioClipStartTime(selectedAudioClip->first,
                                          selectedAudioClip->second,
                                          audioEngine.getPosition());
        updateTimelineSize();
        updateTransportDisplay();
        timelineComponent.repaint();
        return;
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        const auto startBeat = audioEngine.getProjectModel().getTempoMap().secondsToBeats(audioEngine.getPosition());
        audioEngine.setMidiClipStartBeat(selectedMidiClip->first,
                                         selectedMidiClip->second,
                                         startBeat);
        updateTimelineSize();
        updateTransportDisplay();
        timelineComponent.repaint();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before moving it.");
}

void MainComponent::selectAdjacentClip(int direction)
{
    if (! timelineComponent.selectAdjacentClip(direction))
        return;

    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
        selectTrackById(selectedAudioClip->first);

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
        selectTrackById(selectedMidiClip->first);

    timelineComponent.repaint();
}

void MainComponent::selectClipAtPlayhead()
{
    const auto selected = getSelectedTrack();

    if (! timelineComponent.selectClipAtTime(audioEngine.getPosition(), selected.id))
        return;

    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
        selectTrackById(selectedAudioClip->first);

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
        selectTrackById(selectedMidiClip->first);

    timelineComponent.repaint();
}

void MainComponent::alignSelectedClipToBar()
{
    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto beatsPerBar = static_cast<double>(juce::jmax(1, tempo.getNumerator()));

    if (const auto selectedAudioClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedAudioClip->second)
                    continue;

                const auto startBeat = tempo.secondsToBeats(clip.startTimeSeconds);
                const auto alignedBeat = std::round(startBeat / beatsPerBar) * beatsPerBar;
                audioEngine.setAudioClipStartTime(selectedAudioClip->first,
                                                  selectedAudioClip->second,
                                                  tempo.beatsToSeconds(alignedBeat));
                updateTimelineSize();
                timelineComponent.repaint();
                return;
            }
        }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedMidiClip->second)
                    continue;

                const auto alignedBeat = std::round(clip.startBeat / beatsPerBar) * beatsPerBar;
                audioEngine.setMidiClipStartBeat(selectedMidiClip->first,
                                                 selectedMidiClip->second,
                                                 alignedBeat);
                updateTimelineSize();
                timelineComponent.repaint();
                return;
            }
        }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before aligning it to a bar.");
}

void MainComponent::resetSelectedAudioClipSourceStart()
{
    const auto selectedAudioClip = timelineComponent.getSelectedAudioClip();

    if (! selectedAudioClip.has_value())
    {
        showErrorMessage("No audio clip selected", "Select an audio clip before resetting its source start.");
        return;
    }

    if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedAudioClip->first))
    {
        const auto sourceDuration = track->getAudioDurationSeconds();

        for (const auto& clip : track->clips)
        {
            if (clip.id != selectedAudioClip->second)
                continue;

            const auto newLength = juce::jmin(sourceDuration, clip.lengthSeconds + clip.sourceOffsetSeconds);

            if (! audioEngine.setAudioClipTiming(selectedAudioClip->first,
                                                 selectedAudioClip->second,
                                                 clip.startTimeSeconds,
                                                 0.0,
                                                 newLength))
            {
                showErrorMessage("Reset failed", "The selected audio clip source start could not be reset.");
                return;
            }

            updateTimelineSize();
            updateTransportDisplay();
            timelineComponent.repaint();
            return;
        }
    }

    showErrorMessage("Reset failed", "The selected audio clip could not be found.");
}

void MainComponent::importAudioToSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.type != TrackType::Audio)
        return;

    fileChooser = std::make_unique<juce::FileChooser>("Select an audio file",
                                                       juce::File{},
                                                       "*.wav;*.aiff;*.aif;*.mp3");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<MainComponent> safeThis(this);

    fileChooser->launchAsync(flags, [safeThis, selected](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto* component = safeThis.getComponent();
        const auto file = chooser.getResult();

        if (file == juce::File{})
            return;

        if (! component->audioEngine.loadAudioFileToTrack(selected.id, file))
        {
            component->showErrorMessage("Failed to open file",
                                        "The selected audio file could not be loaded.");
            return;
        }

        component->timelineComponent.repaint();
        component->updateTimelineSize();
        component->updateTransportDisplay();
    });
}

void MainComponent::adjustSelectedAudioClipGain(float delta)
{
    const auto selectedAudioClip = timelineComponent.getSelectedAudioClip();

    if (! selectedAudioClip.has_value())
    {
        showErrorMessage("No audio clip selected", "Select an audio clip before changing clip gain.");
        return;
    }

    if (! audioEngine.adjustAudioClipGain(selectedAudioClip->first, selectedAudioClip->second, delta))
    {
        showErrorMessage("Clip gain failed", "The selected audio clip gain could not be changed.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::resetSelectedAudioClipGain()
{
    const auto selectedAudioClip = timelineComponent.getSelectedAudioClip();

    if (! selectedAudioClip.has_value())
    {
        showErrorMessage("No audio clip selected", "Select an audio clip before resetting clip gain.");
        return;
    }

    if (! audioEngine.setAudioClipGain(selectedAudioClip->first, selectedAudioClip->second, 1.0f))
    {
        showErrorMessage("Clip gain failed", "The selected audio clip gain could not be reset.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::normalizeSelectedAudioClip()
{
    const auto selectedAudioClip = timelineComponent.getSelectedAudioClip();

    if (! selectedAudioClip.has_value())
    {
        showErrorMessage("No audio clip selected", "Select an audio clip before normalizing.");
        return;
    }

    if (! audioEngine.normalizeAudioClipGain(selectedAudioClip->first, selectedAudioClip->second))
    {
        showErrorMessage("Normalize failed", "The selected audio clip could not be normalized.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::toggleSelectedAudioClipReverse()
{
    const auto selectedAudioClip = timelineComponent.getSelectedAudioClip();

    if (! selectedAudioClip.has_value())
    {
        showErrorMessage("No audio clip selected", "Select an audio clip before reversing.");
        return;
    }

    if (! audioEngine.toggleAudioClipReversed(selectedAudioClip->first, selectedAudioClip->second))
    {
        showErrorMessage("Reverse failed", "The selected audio clip reverse state could not be changed.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::copySelectedClip()
{
    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        if (audioEngine.getProjectModel().findAudioTrack(selectedClip->first) == nullptr)
        {
            showErrorMessage("Copy failed", "The selected audio clip track no longer exists.");
            return;
        }

        clipClipboard = ClipClipboard { selectedClip->first, selectedClip->second, ClipboardClipType::Audio };
        return;
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first) == nullptr)
        {
            showErrorMessage("Copy failed", "The selected MIDI clip track no longer exists.");
            return;
        }

        clipClipboard = ClipClipboard { selectedMidiClip->first, selectedMidiClip->second, ClipboardClipType::Midi };
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before copying.");
}

void MainComponent::pasteCopiedClipAtPlayhead()
{
    if (! clipClipboard.has_value())
    {
        showErrorMessage("Clipboard empty", "Copy an audio or MIDI clip before pasting.");
        return;
    }

    const auto selectedTrack = getSelectedTrack();

    if (clipClipboard->type == ClipboardClipType::Audio)
    {
        auto destinationTrackId = clipClipboard->sourceTrackId;

        if (selectedTrack.type == TrackType::Audio
            && audioEngine.getProjectModel().findAudioTrack(selectedTrack.id) != nullptr)
        {
            destinationTrackId = selectedTrack.id;
        }

        const auto pastedClipId = audioEngine.duplicateAudioClipToTrackAtTime(clipClipboard->sourceTrackId,
                                                                              destinationTrackId,
                                                                              clipClipboard->clipId,
                                                                              audioEngine.getPosition());

        if (! pastedClipId.has_value())
        {
            showErrorMessage("Paste failed", "The copied audio clip could not be pasted.");
            return;
        }

        selectTrackById(destinationTrackId);
        timelineComponent.selectAudioClip(destinationTrackId, *pastedClipId);
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    auto destinationTrackId = clipClipboard->sourceTrackId;

    if (selectedTrack.type == TrackType::Midi
        && audioEngine.getProjectModel().findMidiTrack(selectedTrack.id) != nullptr)
    {
        destinationTrackId = selectedTrack.id;
    }

    const auto startBeat = audioEngine.getProjectModel().getTempoMap().secondsToBeats(audioEngine.getPosition());
    const auto pastedClipId = audioEngine.duplicateMidiClipToTrackAtBeat(clipClipboard->sourceTrackId,
                                                                         destinationTrackId,
                                                                         clipClipboard->clipId,
                                                                         startBeat);

    if (! pastedClipId.has_value())
    {
        showErrorMessage("Paste failed", "The copied MIDI clip could not be pasted.");
        return;
    }

    selectTrackById(destinationTrackId);
    timelineComponent.selectMidiClip(destinationTrackId, *pastedClipId);
    updateTimelineSize();
    updateTransportDisplay();
}

void MainComponent::duplicateSelectedClip()
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (selectedClip.has_value())
    {
        if (! audioEngine.duplicateAudioClip(selectedClip->first, selectedClip->second))
        {
            showErrorMessage("Duplicate failed", "The selected audio clip could not be duplicated.");
            return;
        }

        timelineComponent.repaint();
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (selectedMidiClip.has_value())
    {
        if (! audioEngine.duplicateMidiClip(selectedMidiClip->first, selectedMidiClip->second))
        {
            showErrorMessage("Duplicate failed", "The selected MIDI clip could not be duplicated.");
            return;
        }

        timelineComponent.repaint();
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before duplicating.");
}

void MainComponent::duplicateSelectedClipAtPlayhead()
{
    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        if (! audioEngine.duplicateAudioClipAtTime(selectedClip->first,
                                                  selectedClip->second,
                                                  audioEngine.getPosition()))
        {
            showErrorMessage("Duplicate failed", "The selected audio clip could not be duplicated at the playhead.");
            return;
        }

        updateTimelineSize();
        updateTransportDisplay();
        timelineComponent.repaint();
        return;
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        const auto startBeat = audioEngine.getProjectModel().getTempoMap().secondsToBeats(audioEngine.getPosition());

        if (! audioEngine.duplicateMidiClipAtBeat(selectedMidiClip->first,
                                                 selectedMidiClip->second,
                                                 startBeat))
        {
            showErrorMessage("Duplicate failed", "The selected MIDI clip could not be duplicated at the playhead.");
            return;
        }

        updateTimelineSize();
        updateTransportDisplay();
        timelineComponent.repaint();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before duplicating.");
}

void MainComponent::duplicateSelectedClipAtNextBar()
{
    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto beatsPerBar = static_cast<double>(juce::jmax(1, tempo.getNumerator()));

    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedClip->second)
                    continue;

                const auto startBeat = tempo.secondsToBeats(clip.startTimeSeconds);
                const auto nextBarBeat = (std::floor(startBeat / beatsPerBar) + 1.0) * beatsPerBar;

                if (! audioEngine.duplicateAudioClipAtTime(selectedClip->first,
                                                           selectedClip->second,
                                                           tempo.beatsToSeconds(nextBarBeat)))
                {
                    showErrorMessage("Duplicate failed", "The selected audio clip could not be duplicated at the next bar.");
                    return;
                }

                updateTimelineSize();
                updateTransportDisplay();
                timelineComponent.repaint();
                return;
            }
        }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedMidiClip->second)
                    continue;

                const auto nextBarBeat = (std::floor(clip.startBeat / beatsPerBar) + 1.0) * beatsPerBar;

                if (! audioEngine.duplicateMidiClipAtBeat(selectedMidiClip->first,
                                                          selectedMidiClip->second,
                                                          nextBarBeat))
                {
                    showErrorMessage("Duplicate failed", "The selected MIDI clip could not be duplicated at the next bar.");
                    return;
                }

                updateTimelineSize();
                updateTransportDisplay();
                timelineComponent.repaint();
                return;
            }
        }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before duplicating it at the next bar.");
}

void MainComponent::duplicateSelectedClipAfterItself()
{
    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedClip->second)
                    continue;

                if (! audioEngine.duplicateAudioClipAtTime(selectedClip->first,
                                                           selectedClip->second,
                                                           clip.startTimeSeconds + clip.lengthSeconds))
                {
                    showErrorMessage("Duplicate failed", "The selected audio clip could not be duplicated after itself.");
                    return;
                }

                updateTimelineSize();
                updateTransportDisplay();
                timelineComponent.repaint();
                return;
            }
        }
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selectedMidiClip->first))
        {
            for (const auto& clip : track->clips)
            {
                if (clip.id != selectedMidiClip->second)
                    continue;

                if (! audioEngine.duplicateMidiClipAtBeat(selectedMidiClip->first,
                                                          selectedMidiClip->second,
                                                          clip.startBeat + clip.lengthBeats))
                {
                    showErrorMessage("Duplicate failed", "The selected MIDI clip could not be duplicated after itself.");
                    return;
                }

                updateTimelineSize();
                updateTransportDisplay();
                timelineComponent.repaint();
                return;
            }
        }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before duplicating it after itself.");
}

void MainComponent::repeatSelectedClipToLoopEnd()
{
    const auto loopRange = audioEngine.getLoopRange();

    if (! loopRange.has_value())
    {
        showErrorMessage("No loop range", "Set a loop range before repeating a clip to the loop end.");
        return;
    }

    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        const auto addedCount = audioEngine.repeatAudioClipUntilTime(selectedClip->first,
                                                                     selectedClip->second,
                                                                     loopRange->second);

        if (addedCount <= 0)
        {
            showInfoMessage("Repeat skipped", "The selected audio clip already reaches the loop end.");
            return;
        }

        updateTimelineSize();
        updateTransportDisplay();
        timelineComponent.repaint();
        return;
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        const auto endBeat = audioEngine.getProjectModel().getTempoMap().secondsToBeats(loopRange->second);
        const auto addedCount = audioEngine.repeatMidiClipUntilBeat(selectedMidiClip->first,
                                                                    selectedMidiClip->second,
                                                                    endBeat);

        if (addedCount <= 0)
        {
            showInfoMessage("Repeat skipped", "The selected MIDI clip already reaches the loop end.");
            return;
        }

        updateTimelineSize();
        updateTransportDisplay();
        timelineComponent.repaint();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before repeating it to the loop end.");
}

void MainComponent::toggleSelectedClipMute()
{
    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        if (! audioEngine.toggleAudioClipMuted(selectedClip->first, selectedClip->second))
            showErrorMessage("Mute failed", "The selected audio clip mute state could not be changed.");

        timelineComponent.repaint();
        return;
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        if (! audioEngine.toggleMidiClipMuted(selectedMidiClip->first, selectedMidiClip->second))
            showErrorMessage("Mute failed", "The selected MIDI clip mute state could not be changed.");

        timelineComponent.repaint();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before muting.");
}

void MainComponent::deleteSelectedClip()
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (selectedClip.has_value())
    {
        if (! audioEngine.deleteAudioClip(selectedClip->first, selectedClip->second))
        {
            showErrorMessage("Delete failed", "The selected audio clip could not be deleted.");
            return;
        }

        timelineComponent.repaint();
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (selectedMidiClip.has_value())
    {
        if (! audioEngine.deleteMidiClip(selectedMidiClip->first, selectedMidiClip->second))
        {
            showErrorMessage("Delete failed", "The selected MIDI clip could not be deleted.");
            return;
        }

        timelineComponent.repaint();
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before deleting.");
}

void MainComponent::splitSelectedClip()
{
    if (const auto selectedClip = timelineComponent.getSelectedAudioClip())
    {
        if (! audioEngine.splitAudioClipAtPosition(selectedClip->first, selectedClip->second, audioEngine.getPosition()))
        {
            showErrorMessage("Split failed", "Move the playhead inside the selected clip before splitting.");
            return;
        }

        timelineComponent.repaint();
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    if (const auto selectedMidiClip = timelineComponent.getSelectedMidiClip())
    {
        const auto splitBeat = audioEngine.getProjectModel().getTempoMap().secondsToBeats(audioEngine.getPosition());

        if (! audioEngine.splitMidiClipAtBeat(selectedMidiClip->first, selectedMidiClip->second, splitBeat))
        {
            showErrorMessage("Split failed", "Move the playhead inside the selected clip before splitting.");
            return;
        }

        timelineComponent.repaint();
        updateTimelineSize();
        updateTransportDisplay();
        return;
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before splitting.");
}

void MainComponent::fadeInSelectedClip()
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (! selectedClip.has_value())
    {
        showErrorMessage("No clip selected", "Select an audio clip before applying a fade.");
        return;
    }

    auto fadeOutSeconds = 0.0;

    if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedClip->first))
        for (const auto& clip : track->clips)
            if (clip.id == selectedClip->second)
                fadeOutSeconds = clip.fadeOutSeconds;

    if (! audioEngine.setAudioClipFade(selectedClip->first, selectedClip->second, 1.0, fadeOutSeconds))
        showErrorMessage("Fade failed", "The selected audio clip could not be faded.");

    timelineComponent.repaint();
}

void MainComponent::fadeOutSelectedClip()
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (! selectedClip.has_value())
    {
        showErrorMessage("No clip selected", "Select an audio clip before applying a fade.");
        return;
    }

    auto fadeInSeconds = 0.0;

    if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedClip->first))
        for (const auto& clip : track->clips)
            if (clip.id == selectedClip->second)
                fadeInSeconds = clip.fadeInSeconds;

    if (! audioEngine.setAudioClipFade(selectedClip->first, selectedClip->second, fadeInSeconds, 1.0))
        showErrorMessage("Fade failed", "The selected audio clip could not be faded.");

    timelineComponent.repaint();
}

void MainComponent::adjustSelectedAudioClipFade(bool fadeIn, double deltaSeconds)
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (! selectedClip.has_value())
    {
        showErrorMessage("No clip selected", "Select an audio clip before adjusting fades.");
        return;
    }

    auto fadeInSeconds = 0.0;
    auto fadeOutSeconds = 0.0;

    if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selectedClip->first))
        for (const auto& clip : track->clips)
            if (clip.id == selectedClip->second)
            {
                fadeInSeconds = clip.fadeInSeconds;
                fadeOutSeconds = clip.fadeOutSeconds;
            }

    if (fadeIn)
        fadeInSeconds = juce::jmax(0.0, fadeInSeconds + deltaSeconds);
    else
        fadeOutSeconds = juce::jmax(0.0, fadeOutSeconds + deltaSeconds);

    if (! audioEngine.setAudioClipFade(selectedClip->first,
                                       selectedClip->second,
                                       fadeInSeconds,
                                       fadeOutSeconds))
    {
        showErrorMessage("Fade failed", "The selected audio clip fade could not be adjusted.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::setSelectedAudioClipFades(double fadeSeconds)
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (! selectedClip.has_value())
    {
        showErrorMessage("No clip selected", "Select an audio clip before setting fades.");
        return;
    }

    const auto safeFadeSeconds = juce::jlimit(0.0, 10.0, fadeSeconds);

    if (! audioEngine.setAudioClipFade(selectedClip->first,
                                       selectedClip->second,
                                       safeFadeSeconds,
                                       safeFadeSeconds))
    {
        showErrorMessage("Fade failed", "The selected audio clip fades could not be set.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::clearSelectedAudioClipFades()
{
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (! selectedClip.has_value())
    {
        showErrorMessage("No clip selected", "Select an audio clip before clearing fades.");
        return;
    }

    if (! audioEngine.clearAudioClipFades(selectedClip->first, selectedClip->second))
    {
        showErrorMessage("Fade failed", "The selected audio clip fades could not be cleared.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::quantizeSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before quantizing.");
        return;
    }

    if (! audioEngine.quantizeMidiClip(selectedMidiClip->first,
                                       selectedMidiClip->second,
                                       timelineComponent.getSnapGridBeats()))
    {
        showErrorMessage("Quantize failed", "The selected MIDI clip could not be quantized.");
        return;
    }

    timelineComponent.repaint();
    updateTransportDisplay();
}

void MainComponent::swingQuantizeSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before swing quantizing.");
        return;
    }

    if (! audioEngine.swingQuantizeMidiClip(selectedMidiClip->first,
                                           selectedMidiClip->second,
                                           timelineComponent.getSnapGridBeats(),
                                           0.55))
    {
        showErrorMessage("Swing failed", "The selected MIDI clip could not be swing quantized.");
        return;
    }

    timelineComponent.repaint();
    updateTransportDisplay();
}

void MainComponent::quantizeSelectedMidiClipToScale(bool minorScale)
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before scale quantizing.");
        return;
    }

    if (! audioEngine.quantizeMidiClipToScale(selectedMidiClip->first, selectedMidiClip->second, minorScale))
    {
        showErrorMessage("Scale failed", "The selected MIDI clip could not be moved to the requested scale.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::transposeSelectedMidiClip(int semitones)
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before transposing.");
        return;
    }

    if (! audioEngine.transposeMidiClip(selectedMidiClip->first, selectedMidiClip->second, semitones))
    {
        showErrorMessage("Transpose failed", "The selected MIDI clip could not be transposed.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::invertSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before inverting notes.");
        return;
    }

    if (! audioEngine.invertMidiClip(selectedMidiClip->first, selectedMidiClip->second))
    {
        showErrorMessage("Invert failed", "The selected MIDI clip could not be inverted.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::layerSelectedMidiClipOctave(int semitones)
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before adding an octave layer.");
        return;
    }

    if (! audioEngine.addMidiClipOctaveLayer(selectedMidiClip->first, selectedMidiClip->second, semitones))
    {
        showErrorMessage("Layer failed", "The selected MIDI clip could not be layered.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::adjustSelectedMidiClipVelocity(float delta)
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before changing velocity.");
        return;
    }

    if (! audioEngine.adjustMidiClipVelocity(selectedMidiClip->first, selectedMidiClip->second, delta))
    {
        showErrorMessage("Velocity edit failed", "The selected MIDI clip velocity could not be changed.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::setSelectedMidiClipVelocity(float velocity)
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before setting velocity.");
        return;
    }

    if (! audioEngine.setMidiClipVelocity(selectedMidiClip->first, selectedMidiClip->second, velocity))
    {
        showErrorMessage("Velocity edit failed", "The selected MIDI clip velocity could not be set.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::accentSelectedMidiClipVelocity()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before adding velocity accents.");
        return;
    }

    if (! audioEngine.accentMidiClipVelocity(selectedMidiClip->first, selectedMidiClip->second))
    {
        showErrorMessage("Accent failed", "The selected MIDI clip velocity could not be accented.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::humanizeSelectedMidiClipVelocity()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before humanizing.");
        return;
    }

    if (! audioEngine.humanizeMidiClipVelocity(selectedMidiClip->first, selectedMidiClip->second, 0.08f))
    {
        showErrorMessage("Humanize failed", "The selected MIDI clip velocity could not be humanized.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::humanizeSelectedMidiClipTiming()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before timing humanize.");
        return;
    }

    if (! audioEngine.humanizeMidiClipTiming(selectedMidiClip->first, selectedMidiClip->second, 0.03))
    {
        showErrorMessage("Humanize failed", "The selected MIDI clip timing could not be humanized.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::legatoSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before applying legato.");
        return;
    }

    if (! audioEngine.legatoMidiClip(selectedMidiClip->first, selectedMidiClip->second))
    {
        showErrorMessage("Legato failed", "The selected MIDI clip could not be made legato.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::staccatoSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before applying staccato.");
        return;
    }

    if (! audioEngine.staccatoMidiClip(selectedMidiClip->first, selectedMidiClip->second))
    {
        showErrorMessage("Staccato failed", "The selected MIDI clip could not be made staccato.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::setSelectedMidiClipNoteLengthToGrid()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before setting note length.");
        return;
    }

    if (! audioEngine.setMidiClipNoteLength(selectedMidiClip->first,
                                           selectedMidiClip->second,
                                           timelineComponent.getSnapGridBeats()))
    {
        showErrorMessage("Length failed", "The selected MIDI clip notes could not be set to the grid length.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::reverseSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before reversing.");
        return;
    }

    if (! audioEngine.reverseMidiClip(selectedMidiClip->first, selectedMidiClip->second))
    {
        showErrorMessage("Reverse failed", "The selected MIDI clip could not be reversed.");
        return;
    }

    timelineComponent.repaint();
}

void MainComponent::generateAiChordsForSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.type != TrackType::Midi)
    {
        showErrorMessage("Select MIDI track", "Select a MIDI track before generating chords.");
        return;
    }

    if (! audioEngine.generateChordProgression(selected.id, "pop"))
    {
        showErrorMessage("AI Chords failed", "Could not generate a MIDI chord progression.");
        return;
    }

    timelineComponent.repaint();
    updateTimelineSize();
    updateTransportDisplay();
}

void MainComponent::generateAiBassForSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.type != TrackType::Midi)
    {
        showErrorMessage("Select MIDI track", "Select a MIDI track before generating a bassline.");
        return;
    }

    if (! audioEngine.generateBassline(selected.id, "pop"))
    {
        showErrorMessage("AI Bass failed", "Could not generate a MIDI bassline.");
        return;
    }

    timelineComponent.repaint();
    updateTimelineSize();
    updateTransportDisplay();
}

void MainComponent::generateAiArpForSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.type != TrackType::Midi)
    {
        showErrorMessage("Select MIDI track", "Select a MIDI track before generating an arpeggio.");
        return;
    }

    if (! audioEngine.generateArpeggio(selected.id, "pop"))
    {
        showErrorMessage("AI Arp failed", "Could not generate a MIDI arpeggio.");
        return;
    }

    timelineComponent.repaint();
    updateTimelineSize();
    updateTransportDisplay();
}

void MainComponent::generateAiDrumsForSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.type != TrackType::Midi)
    {
        showErrorMessage("Select MIDI track", "Select a MIDI track before generating drums.");
        return;
    }

    if (! audioEngine.generateDrumPattern(selected.id, "pop"))
    {
        showErrorMessage("AI Drums failed", "Could not generate a MIDI drum pattern.");
        return;
    }

    timelineComponent.repaint();
    updateTimelineSize();
    updateTransportDisplay();
}

void MainComponent::generateAiMelodyForSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.type != TrackType::Midi)
    {
        showErrorMessage("Select MIDI track", "Select a MIDI track before generating a melody.");
        return;
    }

    if (! audioEngine.generateMelody(selected.id, "pop"))
    {
        showErrorMessage("AI Melody failed", "Could not generate a MIDI melody.");
        return;
    }

    timelineComponent.repaint();
    updateTimelineSize();
    updateTransportDisplay();
}

void MainComponent::exportMix()
{
    fileChooser = std::make_unique<juce::FileChooser>("Export mix",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                           .getChildFile("AI-DAW Mix.wav"),
                                                       "*.wav");
    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;
    juce::Component::SafePointer<MainComponent> safeThis(this);

    fileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto destination = chooser.getResult();

        if (destination == juce::File{})
            return;

        if (! destination.hasFileExtension(".wav"))
            destination = destination.withFileExtension(".wav");

        auto* component = safeThis.getComponent();

        if (component->audioEngine.exportToWav(destination))
            component->showInfoMessage("Export complete", "The mix was exported successfully.");
        else
            component->showErrorMessage("Export failed", "The mix could not be exported.");
    });
}

void MainComponent::exportSelectedTrack()
{
    const auto selected = getSelectedTrack();

    if (selected.id.isNull())
    {
        showErrorMessage("Export failed", "Select a track before exporting a stem.");
        return;
    }

    auto selectedName = juce::String("Selected Track");

    if (selected.type == TrackType::Audio)
    {
        if (const auto* track = audioEngine.getProjectModel().findAudioTrack(selected.id))
            selectedName = track->state.name;
    }
    else if (const auto* track = audioEngine.getProjectModel().findMidiTrack(selected.id))
    {
        selectedName = track->state.name;
    }

    const auto suggestedName = selectedName + " Stem.wav";
    fileChooser = std::make_unique<juce::FileChooser>("Export selected track",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                           .getChildFile(suggestedName),
                                                       "*.wav");
    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;
    juce::Component::SafePointer<MainComponent> safeThis(this);
    const auto trackId = selected.id;

    fileChooser->launchAsync(flags, [safeThis, trackId](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto destination = chooser.getResult();

        if (destination == juce::File{})
            return;

        if (! destination.hasFileExtension(".wav"))
            destination = destination.withFileExtension(".wav");

        auto* component = safeThis.getComponent();

        if (component->audioEngine.exportTrackToWav(trackId, destination))
            component->showInfoMessage("Export complete", "The selected track was exported successfully.");
        else
            component->showErrorMessage("Export failed", "The selected track could not be exported.");
    });
}

void MainComponent::saveProject()
{
    if (currentProjectFile != juce::File{})
    {
        if (audioEngine.saveProject(currentProjectFile))
        {
            updateTitleDisplay();
            showInfoMessage("Project saved", "The project file was saved.");
        }
        else
        {
            showErrorMessage("Project save failed", "The project could not be saved.");
        }

        return;
    }

    saveProjectAs();
}

void MainComponent::saveProjectAs()
{
    fileChooser = std::make_unique<juce::FileChooser>("Save project",
                                                       currentProjectFile != juce::File{}
                                                           ? currentProjectFile
                                                           : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                               .getChildFile("project.aidaw"),
                                                       "*.aidaw");
    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;
    juce::Component::SafePointer<MainComponent> safeThis(this);

    fileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto file = chooser.getResult();

        if (file == juce::File{})
            return;

        if (! file.hasFileExtension(".aidaw"))
            file = file.withFileExtension(".aidaw");

        auto* component = safeThis.getComponent();

        if (component->audioEngine.saveProject(file))
        {
            component->currentProjectFile = file;
            component->updateTitleDisplay();
            component->showInfoMessage("Project saved", "The project file was saved.");
        }
        else
        {
            component->showErrorMessage("Project save failed", "The project could not be saved.");
        }
    });
}

void MainComponent::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Open project", juce::File{}, "*.aidaw");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<MainComponent> safeThis(this);

    fileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto file = chooser.getResult();

        if (file == juce::File{})
            return;

        auto* component = safeThis.getComponent();

        if (component->audioEngine.loadProject(file))
        {
            component->currentProjectFile = file;
            component->updateTitleDisplay();
            component->loopButton.setToggleState(false, juce::dontSendNotification);
            component->timelineComponent.clearLoopRange();
            component->refreshTrackSelector();
            component->timelineComponent.repaint();
            component->updateTimelineSize();
            return;
        }

        component->showErrorMessage("Project open failed", "The project could not be opened.");
    });
}

void MainComponent::showErrorMessage(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, title, message);
}

void MainComponent::showInfoMessage(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, title, message);
}

void MainComponent::selectTrackById(const TrackId& trackId)
{
    const auto& model = audioEngine.getProjectModel();
    auto index = 0;

    for (const auto& track : model.getAudioTracks())
    {
        if (track->state.id == trackId)
        {
            trackSelector.setSelectedItemIndex(index, juce::dontSendNotification);
            updateSelectedTrackControls();
            return;
        }

        ++index;
    }

    for (const auto& track : model.getMidiTracks())
    {
        if (track->state.id == trackId)
        {
            trackSelector.setSelectedItemIndex(index, juce::dontSendNotification);
            updateSelectedTrackControls();
            return;
        }

        ++index;
    }
}

MainComponent::TrackSelection MainComponent::getSelectedTrack() const
{
    auto index = trackSelector.getSelectedItemIndex();
    const auto& model = audioEngine.getProjectModel();

    if (! juce::isPositiveAndBelow(index, trackSelector.getNumItems()))
        index = 0;

    const auto audioCount = static_cast<int>(model.getAudioTracks().size());

    if (index < audioCount && ! model.getAudioTracks().empty())
        return { model.getAudioTracks()[static_cast<size_t>(index)]->state.id, TrackType::Audio };

    const auto midiIndex = index - audioCount;

    if (juce::isPositiveAndBelow(midiIndex, static_cast<int>(model.getMidiTracks().size())))
        return { model.getMidiTracks()[static_cast<size_t>(midiIndex)]->state.id, TrackType::Midi };

    if (! model.getAudioTracks().empty())
        return { model.getAudioTracks().front()->state.id, TrackType::Audio };

    if (! model.getMidiTracks().empty())
        return { model.getMidiTracks().front()->state.id, TrackType::Midi };

    return {};
}

std::optional<int> MainComponent::getComputerKeyboardNoteForKey(int keyCode) const
{
    switch (juce::CharacterFunctions::toLowerCase(static_cast<juce_wchar>(keyCode)))
    {
        case 'a': return 60;
        case 'w': return 61;
        case 's': return 62;
        case 'e': return 63;
        case 'd': return 64;
        case 'f': return 65;
        case 't': return 66;
        case 'g': return 67;
        case 'y': return 68;
        case 'h': return 69;
        case 'u': return 70;
        case 'j': return 71;
        case 'k': return 72;
        default: break;
    }

    return std::nullopt;
}
