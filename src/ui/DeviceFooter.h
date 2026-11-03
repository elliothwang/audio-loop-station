#pragma once

// The strip along the bottom of the window. It shows which audio device is in use and how much
// latency it adds, which is the first thing to check when timing sounds off.

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Describes the current audio device and refreshes whenever the device changes.
class DeviceFooter : public juce::Component, private juce::ChangeListener {
  public:
    // Listens to `deviceManager`, which must outlive the footer.
    explicit DeviceFooter(juce::AudioDeviceManager& deviceManager);

    // Stops listening to the device manager.
    ~DeviceFooter() override;

    // Draws the footer background and its top border.
    void paint(juce::Graphics& g) override;

    // Gives the device description the full width.
    void resized() override;

  private:
    // Refreshes the description when the device manager reports a change.
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Writes the device type, name, sample rate, buffer size, and round trip latency.
    void updateText();

    juce::AudioDeviceManager& deviceManager;
    juce::Label deviceLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceFooter)
};
