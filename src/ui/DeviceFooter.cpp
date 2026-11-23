#include "DeviceFooter.h"

#include "LoopStationLookAndFeel.h"

DeviceFooter::DeviceFooter(juce::AudioDeviceManager& manager) : deviceManager(manager) {
    deviceLabel.setFont(
        juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    deviceLabel.setColour(juce::Label::textColourId, palette::mutedText);
    addAndMakeVisible(deviceLabel);
    timingAdjustmentLabel.setText("Recording adjustment:", juce::dontSendNotification);
    timingAdjustmentLabel.setColour(juce::Label::textColourId, palette::mutedText);
    addAndMakeVisible(timingAdjustmentLabel);

    // Extra compensation to be added to device-reported latency. Editable with +/- buttons
    timingAdjustmentSlider.setSliderStyle(juce::Slider::IncDecButtons);
    timingAdjustmentSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 85, 24);
    timingAdjustmentSlider.setRange(-500.0, 500.0, 1.0);
    timingAdjustmentSlider.setTextValueSuffix(" ms");
    timingAdjustmentSlider.setValue(0.0, juce::dontSendNotification);
    timingAdjustmentSlider.onValueChange = [this]() {
        // Notify MainComponent when the user changes this value.
        if (onTimingAdjustmentChange) {
            onTimingAdjustmentChange(static_cast<int>(timingAdjustmentSlider.getValue()));
        }
    };

    addAndMakeVisible(timingAdjustmentSlider);

    // The device usually opens after the footer exists, so the first real text arrives as a change.
    deviceManager.addChangeListener(this);
    updateText();
}

DeviceFooter::~DeviceFooter() {
    deviceManager.removeChangeListener(this);
}

void DeviceFooter::paint(juce::Graphics& g) {
    g.fillAll(palette::meterTrack);
    g.setColour(palette::border);
    g.fillRect(0, 0, getWidth(), 1);
}

void DeviceFooter::resized() {
    auto area = getLocalBounds().reduced(16, 0);

    // Reserve the right side for the adjustment controls.
    auto adjustmentArea = area.removeFromRight(340);
    timingAdjustmentLabel.setBounds(adjustmentArea.removeFromLeft(170));
    timingAdjustmentSlider.setBounds(adjustmentArea.reduced(0, 6));

    area.removeFromRight(12);
    deviceLabel.setBounds(area);
}

void DeviceFooter::changeListenerCallback(juce::ChangeBroadcaster*) {
    updateText();
}

void DeviceFooter::updateText() {
    juce::AudioIODevice* device = deviceManager.getCurrentAudioDevice();
    const double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    if (sampleRate <= 0.0) {
        deviceLabel.setText("No audio device", juce::dontSendNotification);
        return;
    }

    // Input and output latency together are how late an overdub lands without compensation.
    const int latencySamples =
        device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
    const double latencyMs = 1000.0 * latencySamples / sampleRate;
    const juce::String separator = "   |   ";
    deviceLabel.setText(device->getTypeName() + separator + device->getName() + separator +
                            juce::String(juce::roundToInt(sampleRate)) + " Hz" + separator +
                            juce::String(device->getCurrentBufferSizeSamples()) + " samples" +
                            separator + juce::String(latencyMs, 1) + " ms round trip",
                        juce::dontSendNotification);
}
