#pragma once

#include <JuceHeader.h>

class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override = default;

    // Draws the empty application background for Step 1.
    void paint(juce::Graphics& graphics) override;

    // Keeps the initial placeholder UI responsive during window resizing.
    void resized() override;

private:
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
