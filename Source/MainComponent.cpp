#include "MainComponent.h"

MainComponent::MainComponent()
{
    titleLabel.setText("Mini DAW", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    addAndMakeVisible(titleLabel);

    setSize(1200, 700);
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff202124));
}

void MainComponent::resized()
{
    titleLabel.setBounds(getLocalBounds());
}
