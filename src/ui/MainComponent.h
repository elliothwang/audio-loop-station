#pragma once

#include "AudioCommandQueue.h"
#include "AudioRecorder.h"
#include "DeviceFooter.h"
#include "LiveInputQueue.h"
#include "LoadLoopComponent.h"
#include "LoopStationLookAndFeel.h"
#include "Playhead.h"
#include "TimelineRuler.h"
#include "TrackLane.h"
#include "TransportBar.h"
#include "core/ControllerSnapshot.h"
#include "core/LoopController.h"
#include "core/SampleClock.h"
#include "storage/LoopFile.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

// Declare main UI component, GUI behaviour in MainComponent.cpp
class MainComponent : public juce::AudioAppComponent, private juce::Timer {
  public:
    MainComponent();           // Declare constructor to configure component
    ~MainComponent() override; // Destructor

    void paint(juce::Graphics&) override; // background, custom graphics
    void resized() override;              // position controls

    // AudioAppComponent functions defined in src/audio/MainComponentAudio.cpp
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void
    getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override; // Audio Callback

  private:
    // Declared first so it outlives every component that draws with it.
    LoopStationLookAndFeel lookAndFeel;

    // Shows the icon buttons' tooltips, which stand in for the text they no longer have.
    juce::TooltipWindow tooltipWindow{this};

    TransportBar transportBar;

    // The transport bar owns these buttons. The references keep the names the audio code uses.
    juce::Button& recordButton = transportBar.recordButton();
    juce::Button& playButton = transportBar.playButton();
    juce::Button& stopButton = transportBar.stopButton();
    juce::Button& saveLoopButton = transportBar.saveLoopButton();
    juce::Button& loadLoopButton = transportBar.loadLoopButton();
    juce::Button& settingsButton = transportBar.settingsButton();

    void timerCallback() override;

    // Sets the record button's text, tooltip, and pulse from the recorder's state.
    void updateRecordButton(AudioRecorder::RecordingState recorderState);

    // Feeds the time readout, ruler, and playhead from the playing loop.
    void updateTimeline();

    // Draws the recording take's newest input in its lane, starting the lane over for a new take.
    void updateLiveWaveform();

    // Saves every recorded track as a loop called `name`, and reports how it went.
    void saveLoop(const juce::String& name);

    // Copies every track, where its take starts, and the loop length into `loop`. Fails while a
    // take is recording, before the audio thread has published the newest clip, or if a take,
    // load, or delete finishes during the copy. Message thread.
    [[nodiscard]] juce::Result copyLoop(als::SavedLoop& loop);

    // Loads the loop in `folder` in place of every track, resets every lane's settings, and closes
    // the saved loops panel, or reports a failure and leaves all three alone.
    void loadLoop(const juce::File& folder);

    // Opens the saved loops panel with a fresh list, or closes it, and lays the window out again.
    void showLoopsPanel(bool show);

    // Shows `content` in a dialog called `title` over the window. The dialog is modal, but this
    // returns without waiting for it to close.
    void showDialog(std::unique_ptr<juce::Component> content, const juce::String& title);

    // Returns true if an audio device is running, or shows `title` with why not and returns false.
    bool audioRunning(const juce::String& title);

    // Stops playback and empties selected track, and reports a failure.
    void deleteClip(std::size_t track);

    // Redraws track 1's waveform when a new clip finishes, and clears it while recording.
    void updateWaveform(AudioRecorder::RecordingState recorderState);

    TimelineRuler timelineRuler;
    TrackLane track1{1};
    TrackLane track2{2};
    TrackLane track3{3};
    TrackLane track4{4};

    // The lanes in track order, so track index i drives lanes[i].
    const std::array<TrackLane*, als::kTrackCount> lanes{&track1, &track2, &track3, &track4};
    Playhead playhead; // after the lanes, so it draws over them
    DeviceFooter deviceFooter{deviceManager};
    LoadLoopComponent loopsPanel{[this](const juce::File& folder) { loadLoop(folder); }};

    // ----- Audio, needs declared here -----
    AudioRecorder audioRecorder;
    als::AudioCommandQueue commandQueue;        // UI will send commands to audio thread
    als::LiveInputQueue liveInputQueue;         // Audio thread sends the recording take to the UI
    als::SampleClock sampleClock;               // Clock to track processed samples
    als::ControllerSnapshot controllerSnapshot; // Snapshot will provide state for UI to access
    std::optional<als::LoopController>
        loopController; // Option allows controller to start out absent
    std::array<std::atomic<std::int64_t>, als::kTrackCount>
        trackStartPositions{}; // Loop position where each track started, UI needs to read to align
                               // waveforms

    // Loudest output per track since the timer last read it, from 0 to 1. Audio thread writes,
    // timer resets it.
    std::array<std::atomic<float>, als::kTrackCount> trackPeaks{};

    // Each track's volume gain from 0 to 1, starting at full like the lanes' sliders. The UI
    // writes, the audio thread reads.
    std::array<std::atomic<float>, als::kTrackCount> trackVolumes{
        TrackLane::kDefaultVolume, TrackLane::kDefaultVolume, TrackLane::kDefaultVolume,
        TrackLane::kDefaultVolume};
    static_assert(als::kTrackCount == 4, "trackVolumes needs one starting volume per track");

    // Each track's pan from -1 for full left to 1 for full right, starting at centre. The UI
    // writes, the audio thread reads.
    std::array<std::atomic<float>, als::kTrackCount> trackPans{};

    // Whether each track is muted, starting unmuted. The UI writes, the audio thread reads.
    std::array<std::atomic<bool>, als::kTrackCount> trackMuted{};

    // Tracks start with reverse off. Audio thread will read it when choosing playback sample
    // positions.
    std::array<std::atomic<bool>, als::kTrackCount> trackReversed{};

    // Ramps a track to silence before applying playback direction or slip changes.
    struct TrackPlaybackFade {
        enum class Phase { Steady, FadeOut, FadeIn };

        bool activeReversed = false;
        std::int64_t activeSlipSamples = 0;
        Phase phase = Phase::Steady;
        juce::SmoothedValue<float> gain;
    };
    std::array<TrackPlaybackFade, als::kTrackCount> trackPlaybackFades{};

    // A track's volume and pan gains, eased toward its settings so a change ramps in instead of
    // clicking. Mute eases the volume gain to silence. Audio thread only.
    struct TrackGains {
        juce::SmoothedValue<float> volume;
        juce::SmoothedValue<float> left;
        juce::SmoothedValue<float> right;
    };
    std::array<TrackGains, als::kTrackCount> trackGains;

    // Each tracks slip offset in ms, UI writes, audio thread reads.
    std::array<std::atomic<int>, als::kTrackCount> trackSlips{};

    // Sample rate passed in from prepareToPlay() call, needed for slipMs -> samples
    double audioSampleRate = 0.0;

    // The finished clip count as of the last snapshot publish. Until it matches finishedClips(),
    // the take starts and loop length for the newest clip may not be written yet. Audio thread
    // writes.
    std::atomic<std::uint32_t> publishedClips{0};

    // The finished clip count the waveform was last drawn for. Message thread.
    std::uint32_t waveformClip = 0;
    std::int64_t waveformLoopLength = 0;
    bool waveformSettingsChanged = false; // UI thread, reverse or slip requires waveform redraw

    // The take each lane last drew live, by the clock time it started. Message thread.
    std::array<std::optional<std::int64_t>, als::kTrackCount> liveTakes{};

    // Points each track's gains at its lane's latest volume, pan, and mute, which they ease toward
    // over the next block. Audio thread only.
    void updateGainTargets();

    // Mixes every audible track into `left` and `right` for the `sampleCount` samples from
    // `blockStart`, and holds each track's peak for its meter. If `right` is null, the device has
    // one output and pan is skipped. Audio thread only, and only while loopController is set.
    void mixTracks(const als::LoopTiming& timing, std::int64_t blockStart, int sampleCount,
                   float* left, float* right);

    // Queues the `sampleCount` input samples this block recorded, in chunks that each fit one
    // thumbnail point, so the UI can draw the take as it records. Drops what doesn't fit while the
    // UI is behind. Audio thread only, and only while loopController is set.
    void publishLiveInput(const float* input, int sampleCount);

    void playButtonClicked();
    void stopButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};