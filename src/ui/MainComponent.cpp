#include "MainComponent.h"
#include "SaveLoopComponent.h"

#include <array>
#include <utility>
#include <vector>

namespace {

constexpr int kHeaderHeight = 76; // [px]
constexpr int kRulerHeight = 28;  // [px]
constexpr int kLaneHeight = 118;  // [px]
constexpr int kFooterHeight = 36; // [px]

constexpr int kLoopsPanelWidth = 260; // [px], beside the lanes while open

constexpr int kSettingsWidth = 500;  // [px]
constexpr int kSettingsHeight = 360; // [px]

} // namespace

// MainComponent constructor
MainComponent::MainComponent() {
    // The default look and feel, rather than this component's, so dialogs and tooltips match.
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    addAndMakeVisible(transportBar);
    addAndMakeVisible(deviceFooter);
    // Store the user's extra compensation for future recordings.
    deviceFooter.onTimingAdjustmentChange = [this](int milliseconds) {
        recordingAdjustmentMs.store(milliseconds);
    };
    addAndMakeVisible(timelineRuler);
    addAndMakeVisible(track1);
    addAndMakeVisible(track2);
    addAndMakeVisible(track3);
    addAndMakeVisible(track4);
    addAndMakeVisible(playhead);

    // Save Loop Dialog box handler
    saveLoopButton.onClick = [this]() {
        showDialog(std::make_unique<SaveLoopComponent>(
                       [this](const juce::String& name) { saveLoop(name); }),
                   "Save Loop");
    };

    // One input, since a take records the first input channel, and one or two outputs, since the
    // mixer handles mono and stereo. No MIDI, which the app doesn't use.
    settingsButton.setTooltip("Settings, available while every track is empty");
    settingsButton.onClick = [this]() {
        auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager, /*minInputs*/ 1, /*maxInputs*/ 1, /*minOutputs*/ 1, /*maxOutputs*/ 2,
            /*midiInputs*/ false, /*midiOutput*/ false, /*stereoPairs*/ false,
            /*hideAdvanced*/ false);
        selector->setSize(kSettingsWidth, kSettingsHeight);
        settingsDialog = showDialog(std::move(selector), "Settings");
    };

    // Load opens and closes the saved loops panel, which starts closed so the lanes get the width.
    addChildComponent(loopsPanel);
    loadLoopButton.setColour(juce::TextButton::buttonOnColourId, palette::controlBorder);
    // Light like the loop name, where other toggled buttons use dark text.
    loadLoopButton.setColour(juce::TextButton::textColourOnId, palette::text);
    loadLoopButton.onClick = [this]() { showLoopsPanel(!loopsPanel.isVisible()); };
    showLoopsPanel(false); // gives the button its closed text and tooltip

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

    // The mixer eases each track toward its lane's volume, pan, slip, reverse status, and mute.
    for (std::size_t track = 0; track < lanes.size(); ++track) {
        lanes[track]->onVolumeChange = [this, track](float gain) { trackVolumes[track] = gain; };
        lanes[track]->onPanChange = [this, track](float pan) { trackPans[track] = pan; };
        lanes[track]->onMuteChange = [this, track](bool muted) { trackMuted[track] = muted; };
        lanes[track]->onSlipChange = [this, track](int slip) {
            trackSlips[track] = slip;
            waveformSettingsChanged = true;
        };
        lanes[track]->onReverseChange = [this, track](bool reversed) {
            trackReversed[track].store(reversed);
            waveformSettingsChanged = true;
        };
    }

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
    // Before the device manager goes, which the Settings dialog still listens to.
    delete settingsDialog.getComponent();
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

    // A new sample rate clears every track and a pending load, so devices only change while
    // there's nothing to lose.
    settingsButton.setEnabled(transport == als::Transport::Empty &&
                              recorderState != AudioRecorder::RecordingState::Loading);

    for (std::size_t i = 0; i < lanes.size(); ++i) {
        lanes[i]->setState(controllerSnapshot.track(i));
        lanes[i]->setLevel(trackPeaks[i].exchange(0.0f));
        lanes[i]->setReversed(trackReversed[i].load());
        lanes[i]->setSlip(trackSlips[i].load());
    }

    updateLiveWaveform();
    updateWaveform(recorderState);

    updateTimeline();
}

void MainComponent::updateLiveWaveform() {
    juce::AudioIODevice* device = deviceManager.getCurrentAudioDevice();
    const double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 0.0;

    // Bounded by the queue's size, which also drains every chunk already queued.
    als::LiveInput chunk;
    for (int i = 0; i < als::LiveInputQueue::kCapacity && liveInputQueue.pop(chunk); ++i) {
        if (sampleRate <= 0.0 || chunk.track >= lanes.size()) {
            continue;
        }
        TrackLane& lane = *lanes[chunk.track];
        if (liveTakes[chunk.track] != chunk.take) {
            liveTakes[chunk.track] = chunk.take;
            lane.startLiveWaveform(sampleRate, chunk.loopLength);
        }
        lane.addLiveSamples(chunk.position, chunk.samples.data(), chunk.count);
    }
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

    // Waits for the block that finished the clip to publish its starts and loop length, so the
    // waveforms aren't drawn and cached at the old positions.
    if (publishedClips.load(std::memory_order_acquire) != finished) {
        return;
    }
    const auto loopLength = controllerSnapshot.loopLength();

    // Only redraw waveforms if audio changed, there is new loop length, reverse settings change, or
    // slip adjustment.
    if (recorderState != AudioRecorder::RecordingState::Ready ||
        (finished == waveformClip && loopLength == waveformLoopLength &&
         !waveformSettingsChanged)) {
        return;
    }

    // The acquire above means every live chunk of a finished take is queued by now, so drawing them
    // here keeps any from landing on top of its finished waveform on a later tick.
    updateLiveWaveform();

    std::array<std::vector<float>, als::kTrackCount> samples;
    double sampleRate = 0.0;

    if (!audioRecorder.copyTracks(samples, sampleRate)) {
        return; // UI not able to claim buffers to copy, recorder not in Ready state. Will try
                // again.
    }

    waveformClip = finished;
    waveformLoopLength = loopLength;
    waveformSettingsChanged = false;

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
        const auto trackStart = trackStartPositions[track].load();
        const bool reversed = trackReversed[track].load();
        const auto slipOffsetSamples =
            als::millisecondsToSamples(trackSlips[track].load(), sampleRate);

        // Start full loop of silence with samples set to 0.
        std::vector<float> aligned(length, 0.0f);

        for (std::size_t i = 0; i < clip.size() && i < length; ++i) {
            // Place each sample where it plays in the shared loop with wrapping as needed. Reverse
            // setting will mirror position in loop. Slip applied after reverse. Start is when
            // Record pressed for that track within the shared loop.
            const auto sample = static_cast<std::int64_t>(i); // Cast for signed math
            const auto offset =
                reversed ? loopLength - 1 - sample + slipOffsetSamples : sample + slipOffsetSamples;
            const auto rawPosition = trackStart + offset;

            // double modulo accounts for positive or negative slip
            const auto wrappedPosition = ((rawPosition % loopLength) + loopLength) % loopLength;
            aligned[static_cast<std::size_t>(wrappedPosition)] = clip[i];
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

    case AudioRecorder::RecordingState::Recording:
        recordButton.setButtonText("Finish Recording");
        recordButton.setEnabled(true);
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
    if (loopsPanel.isVisible()) {
        loopsPanel.setBounds(area.removeFromRight(kLoopsPanelWidth));
    }

    timelineRuler.setBounds(area.removeFromTop(kRulerHeight));
    for (TrackLane* lane : {&track1, &track2, &track3, &track4}) {
        lane->setBounds(area.removeFromTop(kLaneHeight));
    }
    playhead.setBounds(timelineRuler.getBounds().getUnion(track4.getBounds()));
}

void MainComponent::saveLoop(const juce::String& name) {
    als::SavedLoop loop;
    juce::Result result = copyLoop(loop);
    const juce::File folder = als::loopFolder(als::defaultLoopsFolder(), name);
    if (result.wasOk()) {
        result = als::saveLoop(loop, folder);
    }

    if (result.failed()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save failed",
                                               result.getErrorMessage());
    } else {
        transportBar.setLoopName(folder.getFileName());
        if (loopsPanel.isVisible()) {
            showLoopsPanel(true); // lists the new loop
        }
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Loop saved",
                                               folder.getFullPathName());
    }
}

void MainComponent::showLoopsPanel(bool show) {
    // Read from disk each time it opens, so loops saved since the last look are listed.
    if (show) {
        loopsPanel.setLoops(als::listLoops(als::defaultLoopsFolder()));
    }
    loopsPanel.setVisible(show);
    loadLoopButton.setToggleState(show, juce::dontSendNotification);

    // While open, Load becomes an X, so it's clear the same button closes the panel.
    loadLoopButton.setButtonText(show ? "X" : "Load");
    loadLoopButton.setTitle(show ? "Close saved loops" : "Load");
    loadLoopButton.setTooltip(show ? "Close saved loops" : "Show saved loops");
    resized();
}

juce::DialogWindow* MainComponent::showDialog(std::unique_ptr<juce::Component> content,
                                              const juce::String& title) {
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content.release());
    options.dialogTitle = title;
    options.dialogBackgroundColour = palette::header;
    options.componentToCentreAround = this; // Centre over main screen
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true; // Use Windows title bar and close button
    options.resizable = false;

    // Modal, so nothing behind it can be pressed, but opens without waiting for it to close.
    return options.launchAsync();
}

juce::Result MainComponent::copyLoop(als::SavedLoop& loop) {
    // Every take, load, and delete finishes a clip. A published count that has caught up means the
    // starts and loop length match the tracks, and an unchanged count means none changed mid-copy.
    const std::uint32_t before = audioRecorder.finishedClips();
    if (publishedClips.load(std::memory_order_acquire) != before) {
        return juce::Result::fail("The loop changed while it was being saved. Try saving again.");
    }
    std::array<std::vector<float>, als::kTrackCount> samples;
    if (!audioRecorder.copyTracks(samples, loop.sampleRate)) {
        return juce::Result::fail("Finish recording before saving.");
    }
    loop.length = controllerSnapshot.loopLength();
    for (std::size_t track = 0; track < als::kTrackCount; ++track) {
        loop.tracks[track].samples = std::move(samples[track]);
        loop.tracks[track].start = trackStartPositions[track].load();
    }
    if (audioRecorder.finishedClips() != before) {
        return juce::Result::fail("The loop changed while it was being saved. Try saving again.");
    }
    return juce::Result::ok();
}

void MainComponent::loadLoop(const juce::File& folder) {
    if (!audioRunning("Load failed")) {
        return;
    }
    als::SavedLoop loop;
    juce::Result result = als::loadLoop(folder, loop);
    if (result.wasOk()) {
        std::array<std::vector<float>, als::kTrackCount> samples;
        std::array<std::int64_t, als::kTrackCount> starts{};
        for (std::size_t track = 0; track < als::kTrackCount; ++track) {
            samples[track] = std::move(loop.tracks[track].samples);
            starts[track] = loop.tracks[track].start;
        }
        result = audioRecorder.loadTracks(std::move(samples), starts, loop.length, loop.sampleRate);
    }

    if (result.failed()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Load failed",
                                               result.getErrorMessage());
        return;
    }
    transportBar.setLoopName(folder.getFileName());
    showLoopsPanel(false); // only once loaded, so after a failure another loop can be picked

    // Loops aren't related, so the new one starts from default lane settings.
    for (TrackLane* lane : lanes) {
        lane->resetSettings();
    }
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
