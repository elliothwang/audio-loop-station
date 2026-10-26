#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Declare track list row UI component, behaviour in TrackListComponent.cpp
class TrackRowComponent : public juce::Component {
  public:
    explicit TrackRowComponent(
        int trackNumber); // Each track row given own trackNumber when created
    void resized() override;

  private:
    void nudgeSlip(int deltaMs); // Move the slip offset and update its readout

    juce::Label nameLabel;
    juce::Label statusLabel;

    juce::Label volumeLabel;
    juce::Slider volumeSlider;

    juce::Label panLabel;
    juce::Slider panSlider;

    juce::ToggleButton reverseButton{"Reverse"};
    juce::Label slipLabel;

    juce::TextButton slipEarlierButton{"-"};
    juce::TextButton slipLaterButton{"+"};
    juce::Label slipOffsetLabel;
    int slipOffsetMs = 0; // [ms], kept here until rows are connected to tracks

    juce::ToggleButton muteButton{"Mute"};
    juce::TextButton deleteTrackButton{"Delete"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackRowComponent)
};