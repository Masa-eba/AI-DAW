#pragma once

#include <JuceHeader.h>

#include <memory>

class MidiInputManager final : private juce::MidiInputCallback
{
public:
    explicit MidiInputManager(juce::MidiKeyboardState& keyboardStateToUse);
    ~MidiInputManager() override;

    juce::Array<juce::MidiDeviceInfo> refreshDevices();
    bool openDevice(const juce::MidiDeviceInfo& device);
    void closeDevice();
    juce::String getCurrentDeviceName() const;

private:
    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override;

    juce::MidiKeyboardState& keyboardState;
    std::unique_ptr<juce::MidiInput> midiInput;
    juce::String currentDeviceName;
};
