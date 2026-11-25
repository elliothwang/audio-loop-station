#pragma once

// One track's lane in the console view. The header on the left holds the track's controls, and
// the rest of the lane is where its waveform is drawn.

#include "IconButton.h"
#include "LevelMeter.h"
#include "core/LoopController.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>
#include <functional>

// A track's name, state, volume, mute, reverse, slip, pan, meter, and delete, laid out in one row.
class TrackLane : public juce::Component {
  public:
    // Width of the controls on the left, shared with the timeline so the playhead lines up.
    static constexpr int kHeaderWidth = 430;

    // The volume gain a lane starts at, which is full.
    static constexpr float kDefaultVolume = 1.0f;

    // Samples per point in the thumbnail, enough detail for a lane's width.
    static constexpr int kSamplesPerThumbnailPoint = 256;

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

    // Starts drawing a take as it records, at `sampleRate`, across a loop of `length` samples. A
    // `length` of 0 is a first take, which grows to fill the lane.
    void startLiveWaveform(double sampleRate, std::int64_t length);

    // Draws `count` samples of the recording take from loop position `position`. They have to fit
    // in one thumbnail point.
    void addLiveSamples(std::int64_t position, const float* samples, int count);

    // Shows a level from 0 to 1 in the lane's meter.
    void setLevel(float level);

    // Update REV button
    void setReversed(bool reversed);

    // Update local slipOffsetMs value & label appearance
    void setSlip(int slipOffsetMs);

    // Puts volume, pan, mute, reverse, and slip back to their defaults, for a newly loaded loop.
    // The volume, pan, mute, reverse, and slip changes are reported through their change callbacks.
    void resetSettings();

    // Called when the lane's delete button is clicked.
    std::function<void()> onDelete;

    // Called with the new gain, from 0 to 1, when the volume slider moves.
    std::function<void(float)> onVolumeChange;

    // Called with the new pan, from -1 for full left to 1 for full right, when the pan knob moves.
    std::function<void(float)> onPanChange;

    // Called with whether the track is now muted, when the mute button is toggled.
    std::function<void(bool)> onMuteChange;

    // Called to report whether a lane's Reverse button is on or off.
    std::function<void(bool)> onReverseChange;

    // Called with the new slip offset
    std::function<void(int)> onSlipChange;

  private:
    // Moves the slip offset and updates its readout.
    void nudgeSlip(int deltaMs);

    // Shows "+" or "-" "<slipOffset>ms"
    void updateSlipLabel(int slipOffset);

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

    // The thumbnail point the recording take is filling. It's added again whole each time, since
    // adding just the new samples would replace the point's earlier peaks with theirs.
    std::array<float, kSamplesPerThumbnailPoint> livePoint{};
    std::int64_t livePointIndex = -1; // -1 until the take's first samples arrive

    juce::Label volumeLabel;
    juce::Slider volumeSlider;
    juce::TextButton muteButton{"M"};
    juce::TextButton reverseButton{"REV"};
    juce::TextButton slipEarlierButton{"-"};
    juce::TextButton slipLaterButton{"+"};
    juce::Label slipOffsetLabel;
    int slipOffsetMs = 0; // kept here to mimick other settings being saved locally by JUCE
    juce::Slider panSlider;
    juce::Label panLabel;
    LevelMeter meter;
    IconButton deleteButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackLane)
};
