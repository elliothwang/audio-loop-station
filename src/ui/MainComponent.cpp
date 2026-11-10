#include "MainComponent.h"
#include "LoadLoopComponent.h"
#include "SaveLoopComponent.h"

#include <array>
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
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Record failed",
                                                   "The audio command queue is full. Try again.");
        }
    };

    track1.onDelete = [this]() { deleteClip(0); };
    track2.onDelete = [this]() { deleteClip(1); };
    track3.onDelete = [this]() { deleteClip(2); };
    track4.onDelete = [this]() { deleteClip(3); };

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

    const auto transport = controllerSnapshot.transport();
    playButton.setEnabled(transport ==
                          als::Transport::Stopped); // Play button enabled if clip finished
    stopButton.setEnabled(
        transport == als::Transport::Playing ||
        transport ==
            als::Transport::RecordingFirstTake); // Stop available during playback or first take

    const std::array<TrackLane*, als::kTrackCount> lanes{&track1, &track2, &track3, &track4};
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        lanes[i]->setState(controllerSnapshot.track(i));
        lanes[i]->setLevel(trackPeaks[i].exchange(0.0f));
    }

    updateWaveform(recorderState);

    updateTimeline();
}

void MainComponent::deleteClip(std::size_t track) {
    if (!audioRunning("Delete failed")) {
        return;
    }

    if (audioRecorder.getState() == AudioRecorder::RecordingState::Recording) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Delete failed",
                                               "Finish recording before deleting a track.");
        return;
    }

    // The audio thread will update the controller and clear the matching buffer. Ctonroller
    // determines if playback should continue or not.
    if (!commandQueue.push({als::Command::DeleteTrack, track})) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Delete failed", "The audio command queue is full.");
    }
}

void MainComponent::updateWaveform(AudioRecorder::RecordingState recorderState) {
    const std::uint32_t finished = audioRecorder.finishedClips();
    const auto loopLength = controllerSnapshot.loopLength();

    // Only redraw waveforms if audio changed or there is new loop length.
    if (recorderState != AudioRecorder::RecordingState::Ready ||
        (finished == waveformClip && loopLength == waveformLoopLength)) {
        return;
    }

    std::array<std::vector<float>, als::kTrackCount> samples;
    double sampleRate = 0.0;

    if (!audioRecorder.copyWaveformTracks(samples, sampleRate)) {
        return; // UI not able to claim buffers to copy, recorder not in Ready state. Will try
                // again.
    }

    waveformClip = finished;
    waveformLoopLength = loopLength;

    // Track lane matches track indices of recorder
    const std::array<TrackLane*, als::kTrackCount> lanes{&track1, &track2, &track3, &track4};

    for (std::size_t track = 0; track < lanes.size(); ++track) {
        auto& clip = samples[track];

        if (clip.empty()) {
            lanes[track]->clearWaveform();
            continue;
        }

        if (loopLength <= 0) {
            lanes[track]->clearWaveform();
            continue;
        }

        const auto length = static_cast<std::size_t>(loopLength);
        const auto start = static_cast<std::size_t>(trackStartPositions[track].load());

        // Start full loop of silence with samples set to 0.
        std::vector<float> aligned(length, 0.0f);

        for (std::size_t i = 0; i < clip.size() && i < length; ++i) {
            // Place each sample where it plays in the shared loop with wrapping as needed. Start is
            // when Record pressed for that track within the shared loop.
            aligned[(start + i) % length] = clip[i];
        }

        // Setwaveform expects Juce buffer, wrap samples so no need to copy again.
        float* channel = aligned.data();
        lanes[track]->setWaveform(
            juce::AudioBuffer<float>(&channel, 1, static_cast<int>(aligned.size())), sampleRate);
    }
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
    const bool playing = controllerSnapshot.transport() == als::Transport::Playing;
    const double length = static_cast<double>(controllerSnapshot.loopLength()) / sampleRate;
    const double position =
        playing ? static_cast<double>(controllerSnapshot.position()) / sampleRate : 0.0;

    transportBar.setTime(position, length);
    timelineRuler.setLength(length);
    playhead.setFraction(playing && length > 0.0 ? position / length : -1.0);
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
