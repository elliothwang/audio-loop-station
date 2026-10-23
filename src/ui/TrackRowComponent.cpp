#include "TrackRowComponent.h"

// TrackRowComponent constructor
TrackRowComponent::TrackRowComponent(int trackNumber) {
    // Create name label with provided track number
    nameLabel.setText(juce::String("Track ") + juce::String(trackNumber),
                      juce::dontSendNotification);

    statusLabel.setText("Empty", juce::dontSendNotification);
    volumeLabel.setText("Volume", juce::dontSendNotification);
    panLabel.setText("Pan", juce::dontSendNotification);

    addAndMakeVisible(nameLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(volumeLabel);
    addAndMakeVisible(panLabel);

    // Format and set default and range values of volume slider
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setRange(0.0, 1.0, 0.01); // limits, adjustement increment
    volumeSlider.setValue(1.0, juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60,
                                 20); // value to right, not read-only, width, height
    addAndMakeVisible(volumeSlider);

    // Format and set default and range values of pan slider
    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setRange(-1.0, 1.0, 0.01); // limits, adjustment increments
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60,
                              20); // value to right, not read-only, width, height
    addAndMakeVisible(panSlider);

    // Reverse Button
    addAndMakeVisible(reverseButton);

    // Slip Controls
    slipLabel.setText("Slip", juce::dontSendNotification);
    slipOffsetLabel.setText("0 ms", juce::dontSendNotification);

    addAndMakeVisible(slipLabel);
    addAndMakeVisible(slipEarlierButton);
    addAndMakeVisible(slipLaterButton);
    addAndMakeVisible(slipOffsetLabel);
    addAndMakeVisible(muteButton);
    addAndMakeVisible(deleteTrackButton);
}

void TrackRowComponent::resized() {
    auto area = getLocalBounds().reduced(10);

    deleteTrackButton.setBounds(area.removeFromRight(100));
    area.removeFromRight(10);

    muteButton.setBounds(area.removeFromRight(70));
    area.removeFromRight(10);

    // Track name and status to left
    nameLabel.setBounds(area.removeFromLeft(70));
    statusLabel.setBounds(area.removeFromLeft(60));

    // Slip and reverse controls on right.
    auto slipArea = area.removeFromRight(250);
    area.removeFromRight(10);

    reverseButton.setBounds(area.removeFromRight(90));
    area.removeFromRight(10);

    // Remaining track row width divide between volume and pan slider controls.
    auto volumeArea = area.removeFromLeft(area.getWidth() / 2);
    volumeArea.removeFromRight(5);

    volumeLabel.setBounds(volumeArea.removeFromLeft(55));
    volumeSlider.setBounds(volumeArea);

    area.removeFromLeft(5);
    panLabel.setBounds(area.removeFromLeft(35));
    panSlider.setBounds(area);

    // Slip controls.
    slipLabel.setBounds(slipArea.removeFromLeft(35));
    slipEarlierButton.setBounds(slipArea.removeFromLeft(65));
    slipArea.removeFromLeft(5);

    slipOffsetLabel.setBounds(slipArea.removeFromLeft(65));
    slipLaterButton.setBounds(slipArea.removeFromLeft(65));
}