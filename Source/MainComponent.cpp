#include "MainComponent.h"

MainComponent::MainComponent()
{
    titleLabel.setText("Mini DAW", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    fileNameLabel.setText("No file loaded", juce::dontSendNotification);
    fileNameLabel.setJustificationType(juce::Justification::centred);
    fileNameLabel.setFont(juce::Font(juce::FontOptions(18.0f)));
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    fileNameLabel.setMinimumHorizontalScale(0.5f);
    addAndMakeVisible(fileNameLabel);

    openButton.setButtonText("Open");
    openButton.onClick = [this] { openAudioFile(); };
    addAndMakeVisible(openButton);

    playPauseButton.setButtonText("Play");
    playPauseButton.onClick = [this]
    {
        if (audioEngine.isPlaying())
        {
            audioEngine.pause();
            playPauseButton.setButtonText("Play");
        }
        else
        {
            audioEngine.play();
            playPauseButton.setButtonText(audioEngine.isPlaying() ? "Pause" : "Play");
        }

        updateButtonStates();
    };
    addAndMakeVisible(playPauseButton);

    stopButton.setButtonText("Stop");
    stopButton.onClick = [this]
    {
        audioEngine.stop();
        playPauseButton.setButtonText("Play");
        updateButtonStates();
    };
    addAndMakeVisible(stopButton);

    audioSourcePlayer.setSource(&audioEngine);

    const auto audioDeviceError = audioDeviceManager.initialiseWithDefaultDevices(0, 2);

    if (audioDeviceError.isNotEmpty())
        showErrorMessage("Audio device error", audioDeviceError);
    else
        audioDeviceManager.addAudioCallback(&audioSourcePlayer);
    updateButtonStates();

    setSize(1200, 700);
}

MainComponent::~MainComponent()
{
    audioEngine.stop();
    audioDeviceManager.removeAudioCallback(&audioSourcePlayer);
    audioSourcePlayer.setSource(nullptr);
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff202124));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(40);

    titleLabel.setBounds(area.removeFromTop(80));
    area.removeFromTop(80);
    fileNameLabel.setBounds(area.removeFromTop(50));
    area.removeFromTop(50);

    auto buttonArea = area.removeFromTop(50);
    const auto buttonWidth = 110;
    const auto buttonHeight = 36;
    const auto gap = 24;
    const auto totalWidth = (buttonWidth * 3) + (gap * 2);
    auto x = buttonArea.getCentreX() - (totalWidth / 2);
    const auto y = buttonArea.getCentreY() - (buttonHeight / 2);

    openButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + gap;
    playPauseButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + gap;
    stopButton.setBounds(x, y, buttonWidth, buttonHeight);
}

void MainComponent::openAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an audio file",
        juce::File{},
        "*.wav;*.aiff;*.aif;*.mp3");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    juce::Component::SafePointer<MainComponent> safeThis(this);

    fileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto* component = safeThis.getComponent();
        const auto selectedFile = chooser.getResult();

        if (selectedFile == juce::File{})
            return;

        if (component->audioEngine.loadFile(selectedFile))
        {
            component->fileNameLabel.setText(selectedFile.getFileName(), juce::dontSendNotification);
            component->playPauseButton.setButtonText("Play");
            component->updateButtonStates();
            return;
        }

        component->showErrorMessage("Failed to open file",
                                    "The selected audio file could not be loaded.");
        component->updateButtonStates();
    });
}

void MainComponent::updateButtonStates()
{
    const auto hasFile = audioEngine.hasLoadedFile();

    openButton.setEnabled(true);
    playPauseButton.setEnabled(hasFile);
    stopButton.setEnabled(hasFile);

    if (! hasFile)
        playPauseButton.setButtonText("Play");
    else if (audioEngine.isPlaying())
        playPauseButton.setButtonText("Pause");
}

void MainComponent::showErrorMessage(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                           title,
                                           message);
}
