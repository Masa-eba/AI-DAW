#include "MainComponent.h"

#include "TimeFormatter.h"

#include <cmath>

MainComponent::MainComponent()
    : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      midiInputManager(keyboardState)
{
    audioEngine.setMidiKeyboardState(&keyboardState);

    titleLabel.setText("AI-DAW", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    positionLabel.setText("00:00 / 00:00   Bar 1 Beat 1", juce::dontSendNotification);
    positionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(positionLabel);

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmLabel);

    masterLabel.setText("Master", juce::dontSendNotification);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(masterLabel);

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

    addAudioTrackButton.setButtonText("+ Audio Track");
    addAudioTrackButton.onClick = [this]
    {
        audioEngine.addAudioTrack();
        refreshTrackSelector();
    };
    addAndMakeVisible(addAudioTrackButton);

    addMidiTrackButton.setButtonText("+ MIDI Track");
    addMidiTrackButton.onClick = [this]
    {
        audioEngine.addMidiTrack();
        refreshTrackSelector();
    };
    addAndMakeVisible(addMidiTrackButton);

    renameTrackButton.setButtonText("Rename");
    renameTrackButton.onClick = [this] { renameSelectedTrack(); };
    addAndMakeVisible(renameTrackButton);

    duplicateTrackButton.setButtonText("Dup Track");
    duplicateTrackButton.onClick = [this]
    {
        const auto selected = getSelectedTrack();

        if (audioEngine.duplicateTrack(selected.id).isNull())
        {
            showErrorMessage("Duplicate failed", "The selected track could not be duplicated.");
            return;
        }

        refreshTrackSelector();
        updateTimelineSize();
        timelineComponent.repaint();
    };
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

    snapButton.setButtonText("Snap");
    snapButton.setClickingTogglesState(true);
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this]
    {
        timelineComponent.setSnapEnabled(snapButton.getToggleState());
    };
    addAndMakeVisible(snapButton);

    aiChordsButton.setButtonText("AI Chords");
    aiChordsButton.onClick = [this] { generateAiChordsForSelectedTrack(); };
    addAndMakeVisible(aiChordsButton);

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
            audioEngine.clearLoopRange();
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
    snapButton.setBounds(clipBar.removeFromLeft(62).reduced(0, 5));
    clipBar.removeFromLeft(8);
    aiChordsButton.setBounds(clipBar.removeFromLeft(92).reduced(0, 5));

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
    zoomLabel.setBounds(midiBar.removeFromLeft(48));
    zoomSlider.setBounds(midiBar.removeFromLeft(220).reduced(0, 4));

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

    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        panicAllNotes();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'd')
    {
        duplicateSelectedClip();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'e')
    {
        splitSelectedClip();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'k')
    {
        quantizeSelectedMidiClip();
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
        component->loopButton.setToggleState(false, juce::dontSendNotification);
        component->refreshTrackSelector();
        component->updateTimelineSize();
        component->updateTransportDisplay();
        component->timelineComponent.repaint();
    });
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
                    audioEngine.setPosition(startSeconds);
                    loopButton.setToggleState(true, juce::dontSendNotification);
                    updateTransportDisplay();
                    timelineComponent.repaint();
                    return;
                }
    }

    showErrorMessage("No clip selected", "Select an audio or MIDI clip before setting a clip loop.");
}

void MainComponent::nudgeSelectedClip(int direction)
{
    if (direction == 0)
        return;

    const auto& tempo = audioEngine.getProjectModel().getTempoMap();
    const auto stepBeats = 0.25;
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
    const auto selectedClip = timelineComponent.getSelectedAudioClip();

    if (! selectedClip.has_value())
    {
        showErrorMessage("No clip selected", "Select an audio clip before splitting.");
        return;
    }

    if (! audioEngine.splitAudioClipAtPosition(selectedClip->first, selectedClip->second, audioEngine.getPosition()))
    {
        showErrorMessage("Split failed", "Move the playhead inside the selected clip before splitting.");
        return;
    }

    timelineComponent.repaint();
    updateTimelineSize();
    updateTransportDisplay();
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

void MainComponent::quantizeSelectedMidiClip()
{
    const auto selectedMidiClip = timelineComponent.getSelectedMidiClip();

    if (! selectedMidiClip.has_value())
    {
        showErrorMessage("No MIDI clip selected", "Select a MIDI clip before quantizing.");
        return;
    }

    if (! audioEngine.quantizeMidiClip(selectedMidiClip->first, selectedMidiClip->second, 0.25))
    {
        showErrorMessage("Quantize failed", "The selected MIDI clip could not be quantized.");
        return;
    }

    timelineComponent.repaint();
    updateTransportDisplay();
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

void MainComponent::saveProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Save project",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
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
            component->showInfoMessage("Project saved", "The project file was saved.");
        else
            component->showErrorMessage("Project save failed", "The project could not be saved.");
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
