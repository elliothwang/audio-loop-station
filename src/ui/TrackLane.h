#pragma once

// One track's lane in the console view. The header on the left holds the track's controls, and
// the rest of the lane is where its waveform is drawn.

#include "IconButton.h"
#include "LevelMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

// A track's name, state, volume, mute, reverse, slip, pan, meter, and delete, laid out in one row.
class TrackLane : public juce::Component {
  public:
    // Width of the controls on the left, shared with the timeline so the playhead lines up.
    static constexpr int kHeaderWidth = 430;

    // Lanes are numbered from 1 for display.
    explicit TrackLane(int trackNumber);

    // Draws the lane background, its dividers, and the empty waveform area.
    void paint(juce::Graphics& g) override;

    // Positions the controls in the header.
    void resized() override;

  private:
    // Moves the slip offset and updates its readout.
    void nudgeSlip(int deltaMs);

    // Shows the pan position as C, or how far left or right it is.
    void updatePanLabel();

    juce::Label numberLabel;
    juce::Label nameLabel;
    juce::Label stateLabel;
    juce::Label volumeLabel;
    juce::Slider volumeSlider;
    juce::TextButton muteButton{"M"};
    juce::TextButton reverseButton{"REV"};
    juce::TextButton slipEarlierButton{"-"};
    juce::TextButton slipLaterButton{"+"};
    juce::Label slipOffsetLabel;
    int slipOffsetMs = 0; // [ms], kept here until lanes are connected to tracks
    juce::Slider panSlider;
    juce::Label panLabel;
    LevelMeter meter;
    IconButton deleteButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackLane)
};
