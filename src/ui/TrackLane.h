#pragma once

// One track's lane in the console view. The header on the left holds the track's controls, and
// the rest of the lane is where its waveform is drawn.

#include "IconButton.h"
#include "LevelMeter.h"
#include "core/LoopController.h"

#include <juce_audio_utils/juce_audio_utils.h>
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

    // Shows whether the track is empty, recording, or holds a clip, in the badge and the waveform
    // area. Repaints only on a change.
    void setState(als::TrackState newState);

    // Draws the mono `clip`, recorded at `sampleRate`, as the track's waveform.
    void setWaveform(const juce::AudioBuffer<float>& clip, double sampleRate);

    // Removes the waveform, so the lane shows its empty outline.
    void clearWaveform();

  private:
    // Samples per point in the thumbnail, enough detail for a lane's width.
    static constexpr int kSamplesPerThumbnailPoint = 256;

    // Moves the slip offset and updates its readout.
    void nudgeSlip(int deltaMs);

    // Shows the pan position as C, or how far left or right it is.
    void updatePanLabel();

    juce::Label numberLabel;
    juce::Label nameLabel;
    juce::Label stateLabel;
    als::TrackState state = als::TrackState::Empty;

    // The thumbnail is fed from memory, so the format manager and cache are only there because
    // its constructor needs them.
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache{1};
    juce::AudioThumbnail thumbnail{kSamplesPerThumbnailPoint, formatManager, thumbnailCache};

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
