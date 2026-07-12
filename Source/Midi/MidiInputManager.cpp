#include "MidiInputManager.h"

MidiInputManager::MidiInputManager(juce::MidiKeyboardState& keyboardStateToUse)
    : keyboardState(keyboardStateToUse)
{
}

MidiInputManager::~MidiInputManager()
{
    closeDevice();
}

juce::Array<juce::MidiDeviceInfo> MidiInputManager::refreshDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

bool MidiInputManager::openDevice(const juce::MidiDeviceInfo& device)
{
    closeDevice();
    midiInput = juce::MidiInput::openDevice(device.identifier, this);

    if (midiInput == nullptr)
        return false;

    currentDeviceName = device.name;
    midiInput->start();
    return true;
}

void MidiInputManager::closeDevice()
{
    if (midiInput != nullptr)
        midiInput->stop();

    midiInput.reset();
    currentDeviceName.clear();
}

juce::String MidiInputManager::getCurrentDeviceName() const
{
    return currentDeviceName;
}

void MidiInputManager::handleIncomingMidiMessage(juce::MidiInput* source,
                                                 const juce::MidiMessage& message)
{
    keyboardState.processNextMidiEvent(message);
    juce::ignoreUnused(source);
}
