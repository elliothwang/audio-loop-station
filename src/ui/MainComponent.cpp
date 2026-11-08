#include "MainComponent.h"
#include "LoadLoopComponent.h"
#include "SaveLoopComponent.h"

#include <vector>

namespace {

constexpr int kHeaderHeight = 76; // [px]
constexpr int kRulerHeight = 28;  // [px]
constexpr int kLaneHeight = 118;  // [px]
constexpr int kFooterHeight = 36; // [px]

} // namespace

// MainComponent constructor
MainComponent::MainComponent() {
    // The default look and feel, rather than this component's, so dialogs and tooltips match.
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    addAndMakeVisible(transportBar);
    addAndMakeVisible(deviceFooter);

    addAndMakeVisible(timelineRuler);
    addAndMakeVisible(track1);
    addAndMakeVisible(track2);
    addAndMakeVisible(track3);
    addAndMakeVisible(track4);
    addAndMakeVisible(playhead);

    // Save Loop Dialog box handler
    saveLoopButton.onClick = [this]() {
        juce::DialogWindow::LaunchOptions options;

        // Create dialog window's contents with SaveLoopComponent
        options.content.setOwned(
            new SaveLoopComponent([this](const juce::String& name) { saveLoop(name); }));
        options.dialogTitle = "Save Loop";
        options.dialogBackgroundColour = palette::header;
        options.componentToCentreAround = this; // Centre over main screen
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true; // Use Windows title bar and close button
        options.resizable = false;

        options
            .launchAsync(); // Function can open the dialog box without waiting for user to close.
    };

    // Load previously save loop Dialog Box Handler
    loadLoopButton.onClick = [this]() {
        juce::DialogWindow::LaunchOptions options;

        options.content.setOwned(
            new LoadLoopComponent(als::listLoops(als::defaultLoopsFolder()),
                                  [this](const juce::File& folder) { loadLoop(folder); }));
        options.dialogTitle = "Load Loop";
        options.dialogBackgroundColour = palette::header;
        options.componentToCentreAround = this;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    };

    setSize(1280, kHeaderHeight + kRulerHeight + 4 * kLaneHeight + kFooterHeight);

    // ----- Audio work required here -----
    setAudioChannels(1, 2); // 1 input for recording, 2 output channels for playback

    recordButton.onClick = [this]() {
        if (!audioRunning("Record failed")) {
            return;
        }

        if (!commandQueue.push({als::Command::Record, 0})) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon, "Record failed",
                "The audio command queue is full. Try again.");
            }
        };

    track1.onDelete = [this]() { deleteClip(); };

    // Audio button 'clicked' functions defined in src/audio/MainComponentAudio.cpp
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setEnabled(false); // is enabled once a clip is recorded
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setEnabled(false);

    // Requests 20 UI updates/s
    startTimerHz(20);

    // Children were built before the look and feel was installed, and some copy its colours when
    // they're created, so tell them all again now.
    sendLookAndFeelChange();
}

MainComponent::~MainComponent() {
    stopTimer();
    shutdownAudio();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
};

void MainComponent::timerCallback() {
    const auto recorderState = audioRecorder.getState();
    updateRecordButton(recorderState);

    // Play needs a finished clip, and the player only reports a length once there is one.
    const bool hasClip = loopPlayer.length() > 0;
    const bool playing = loopPlayer.playing();
    playButton.setEnabled(hasClip && !playing);
    stopButton.setEnabled(playing);

    track1.setState(controllerSnapshot.track(0));
    track2.setState(controllerSnapshot.track(1));
    track3.setState(controllerSnapshot.track(2));
    track4.setState(controllerSnapshot.track(3));

    updateWaveform(recorderState);
    track1.setLevel(playbackLevel.exchange(0.0f));

    updateTimeline();
}

void MainComponent::deleteClip() {
    if (!audioRunning("Delete failed")) {
        return;
    }
    loopPlayer.stop();
    if (const juce::Result result = audioRecorder.clearClip(); result.failed()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Delete failed", result.getErrorMessage());
        return;
    }
    // The saved loop on disk is untouched, but the app no longer holds it.
    transportBar.setLoopName(TransportBar::kUntitledLoop);
}

void MainComponent::updateWaveform(AudioRecorder::RecordingState recorderState) {
    if (recorderState == AudioRecorder::RecordingState::Recording) {
        track1.clearWaveform(); // the old clip is being replaced
        return;
    }
    // A count rather than the state, since a load returns to Ready faster than the timer ticks.
    const std::uint32_t finished = audioRecorder.finishedClips();
    if (finished == waveformClip || recorderState != AudioRecorder::RecordingState::Ready) {
        return;
    }
    waveformClip = finished;
    std::vector<float> samples;
    double sampleRate = 0.0;
    if (audioRecorder.copyRecording(samples, sampleRate).failed()) {
        track1.clearWaveform(); // a take stopped before it captured anything
        return;
    }
    // The buffer points at the copied samples rather than copying them again.
    float* channel = samples.data();
    track1.setWaveform(juce::AudioBuffer<float>(&channel, 1, static_cast<int>(samples.size())),
                       sampleRate);
}

void MainComponent::updateRecordButton(AudioRecorder::RecordingState recorderState) {
    switch (recorderState) // Get snapshot or recorder's current state. Set buttons appropriately.
    {
    case AudioRecorder::RecordingState::Idle:
    case AudioRecorder::RecordingState::Ready:
        recordButton.setButtonText("Record");
        recordButton.setEnabled(true);
        break;

    case AudioRecorder::RecordingState::StartRequested:
        recordButton.setButtonText("Starting...");
        recordButton.setEnabled(true);
        break;

    case AudioRecorder::RecordingState::Recording:
        recordButton.setButtonText("Finish Recording");
        recordButton.setEnabled(true);
        break;

    case AudioRecorder::RecordingState::StopRequested:
        recordButton.setButtonText("Stopping...");
        recordButton.setEnabled(false);
        break;
    case AudioRecorder::RecordingState::Saving:
        recordButton.setButtonText("Saving...");
        recordButton.setEnabled(false);
        break;
    case AudioRecorder::RecordingState::Loading:
        recordButton.setButtonText("Loading...");
        recordButton.setEnabled(false);
        break;
    }

    // The record icon has no text, so its state shows in the tooltip and as a pulsing fill.
    recordButton.setTooltip(recordButton.getButtonText());
    recordButton.setToggleState(recorderState == AudioRecorder::RecordingState::Recording,
                                juce::dontSendNotification);
    if (recordButton.getToggleState()) {
        recordButton.repaint(); // keeps the pulse moving
    }
}

void MainComponent::updateTimeline() {
    juce::AudioIODevice* device = deviceManager.getCurrentAudioDevice();
    const double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    if (sampleRate <= 0.0) {
        return;
    }
    const double length = static_cast<double>(loopPlayer.length()) / sampleRate;
    const double position = static_cast<double>(loopPlayer.position()) / sampleRate;
    transportBar.setTime(position, length);
    timelineRuler.setLength(length);
    playhead.setFraction(loopPlayer.playing() && length > 0.0 ? position / length : -1.0);
}

void MainComponent::paint(juce::Graphics& graphics) {
    // fill background
    graphics.fillAll(palette::background);
}

// Called initially and when main components size changed.
void MainComponent::resized() {
    auto area = getLocalBounds();
    transportBar.setBounds(area.removeFromTop(kHeaderHeight));

    deviceFooter.setBounds(area.removeFromBottom(kFooterHeight));

    timelineRuler.setBounds(area.removeFromTop(kRulerHeight));
    for (TrackLane* lane : {&track1, &track2, &track3, &track4}) {
        lane->setBounds(area.removeFromTop(kLaneHeight));
    }
    playhead.setBounds(timelineRuler.getBounds().getUnion(track4.getBounds()));
}

void MainComponent::saveLoop(const juce::String& name) {
    als::SavedLoop loop;
    juce::Result result = audioRecorder.copyRecording(loop.samples, loop.sampleRate);
    const juce::File folder = als::loopFolder(als::defaultLoopsFolder(), name);
    if (result.wasOk()) {
        result = als::saveLoop(loop, folder);
    }

    if (result.failed()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save failed",
                                               result.getErrorMessage());
    } else {
        transportBar.setLoopName(folder.getFileName());
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Loop saved",
                                               folder.getFullPathName());
    }
}

void MainComponent::loadLoop(const juce::File& folder) {
    if (!audioRunning("Load failed")) {
        return;
    }
    als::SavedLoop loop;
    juce::Result result = als::loadLoop(folder, loop);
    if (result.wasOk()) {
        result = audioRecorder.loadClip(loop.samples, loop.sampleRate);
    }

    if (result.failed()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Load failed",
                                               result.getErrorMessage());
        return;
    }
    transportBar.setLoopName(folder.getFileName());
}

bool MainComponent::audioRunning(const juce::String& title) {
    // The audio thread finishes loads and deletes, so without a device they would never complete.
    juce::AudioIODevice* device = deviceManager.getCurrentAudioDevice();
    if (device != nullptr && device->isPlaying()) {
        return true;
    }
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title,
                                           "No audio device is running.");
    return false;
}
