#include "MainComponent.h"
#include "LoadLoopComponent.h"
#include "SaveLoopComponent.h"

namespace {

constexpr int kHeaderHeight = 76; // [px]
constexpr int kRulerHeight = 28;  // [px]
constexpr int kLaneHeight = 118;  // [px]
constexpr int kFooterHeight = 36; // [px]

} // namespace

// MainComponent constructor
MainComponent::MainComponent() : audioState(Stopped) {
    // The default look and feel, rather than this component's, so dialogs and tooltips match.
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    addAndMakeVisible(transportBar);
    addAndMakeVisible(deviceFooter);
    addAndMakeVisible(loadWavButton); // after the footer, so the buttons draw on top of it
    addAndMakeVisible(saveClipButton);

    addAndMakeVisible(timelineRuler);
    addAndMakeVisible(track1);
    addAndMakeVisible(track2);
    addAndMakeVisible(track3);
    addAndMakeVisible(track4);
    addAndMakeVisible(playhead);

    // Save Loop Dialog box handler
    saveLoopButton.onClick = [this]() {
        juce::DialogWindow::LaunchOptions options;

        options.content.setOwned(
            new SaveLoopComponent()); // Create dialog window's contents with SaveLoopComponent
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

        options.content.setOwned(new LoadLoopComponent());
        options.dialogTitle = "Load Loop";
        options.dialogBackgroundColour = palette::header;
        options.componentToCentreAround = this;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    };

    saveClipButton.onClick = [this]() { saveClipButtonClicked(); };

    setSize(1280, kHeaderHeight + kRulerHeight + 4 * kLaneHeight + kFooterHeight);

    // ----- Audio work required here -----
    setAudioChannels(1, 2); // 1 input for recording, 2 output channels for playback
    formatManager.registerBasicFormats();
    transportSource.addChangeListener(this);

    recordButton.onClick = [this]() {
        const auto currentState = audioRecorder.getState();

        switch (currentState) {
        // If recorder state "Idle" or "Ready", request new recording
        case AudioRecorder::RecordingState::Idle:
        case AudioRecorder::RecordingState::Ready:
            audioRecorder.requestStart();
            break;

        // If recorder state "StartRequested or Recording", request stop
        case AudioRecorder::RecordingState::StartRequested:
        case AudioRecorder::RecordingState::Recording:
            audioRecorder.requestStop();
            break;

        // Do nothing while saving
        case AudioRecorder::RecordingState::Saving:
            break;

        // Do nothing while audio thread finsishes stopping
        case AudioRecorder::RecordingState::StopRequested:
            break;
        }
    };

    // Audio button 'clicked' functions defined in src/audio/MainComponentAudio.cpp
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setEnabled(false); // is enabled once user selects .wav
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setEnabled(false);
    loadWavButton.onClick = [this] { loadWavButtonClicked(); };

    // Requests 20 UI updates/s
    startTimerHz(20);
}

MainComponent::~MainComponent() {
    stopTimer();
    shutdownAudio();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
};

void MainComponent::timerCallback() {
    const auto recorderState = audioRecorder.getState();
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
    }

    // The record icon has no text, so its state shows in the tooltip and as a pulsing fill.
    recordButton.setTooltip(recordButton.getButtonText());
    recordButton.setToggleState(recorderState == AudioRecorder::RecordingState::Recording,
                                juce::dontSendNotification);
    if (recordButton.getToggleState()) {
        recordButton.repaint(); // keeps the pulse moving
    }

    updateTimeline();
}

void MainComponent::updateTimeline() {
    const bool loaded = readerSource != nullptr;
    const double length = loaded ? transportSource.getLengthInSeconds() : 0.0;
    const double position = loaded ? transportSource.getCurrentPosition() : 0.0;
    transportBar.setTime(position, length);
    timelineRuler.setLength(length);
    playhead.setFraction(length > 0.0 ? position / length : -1.0);
}

void MainComponent::paint(juce::Graphics& graphics) {
    // fill background
    graphics.fillAll(palette::background);
}

// Called initially and when main components size changed.
void MainComponent::resized() {
    auto area = getLocalBounds();
    transportBar.setBounds(area.removeFromTop(kHeaderHeight));

    auto footer = area.removeFromBottom(kFooterHeight);
    deviceFooter.setBounds(footer);
    // The temporary .wav buttons sit at the footer's right end until loops replace them.
    auto temporaryButtons = footer.reduced(12, 5);
    saveClipButton.setBounds(temporaryButtons.removeFromRight(100));
    temporaryButtons.removeFromRight(8);
    loadWavButton.setBounds(temporaryButtons.removeFromRight(100));

    timelineRuler.setBounds(area.removeFromTop(kRulerHeight));
    for (TrackLane* lane : {&track1, &track2, &track3, &track4}) {
        lane->setBounds(area.removeFromTop(kLaneHeight));
    }
    playhead.setBounds(timelineRuler.getBounds().getUnion(track4.getBounds()));
}

void MainComponent::saveClipButtonClicked() {
    // Check if recording finished
    if (audioRecorder.getState() != AudioRecorder::RecordingState::Ready) {
        // Alert warning if saving before ready. TODO: update pending application design choices.
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save Clip",
                                               "Record a clip and finish recording before saving.");
        return;
    }

    // Create file chooser, suggested location, .wav filetype filter
    saveClipChooser = std::make_unique<juce::FileChooser>(
        "Save recorded clip",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("recording.wav"),
        "*.wav");

    // Chooser permissions. Opens in save mode, user chooses file, warning if file already exists.
    const int saveChooserFlags = juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles |
                                 juce::FileBrowserComponent::warnAboutOverwriting;

    // Open chooser, async open dialog and return without waiting
    saveClipChooser->launchAsync(saveChooserFlags, [this](const juce::FileChooser& fileChooser) {
        const auto destination = fileChooser.getResult();

        // An empty result means the user cancelled.
        if (destination == juce::File{}) // Retrieve user chosen file path
        {
            return;
        }

        const auto result = audioRecorder.saveToWav(destination); // writes WAV file, returns result

        // Alerts, TO UPDATE pending app design choice
        if (result.failed()) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Save failed", result.getErrorMessage());
        } else {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Clip saved",
                                                   destination.getFullPathName());
        }
    });
}