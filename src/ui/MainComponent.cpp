#include "MainComponent.h"
#include "LoadLoopComponent.h"
#include "SaveLoopComponent.h"

// MainComponent constructor
MainComponent::MainComponent() : audioState(Stopped) {
    titleLabel.setText("Audio Loop Station - UI ready", juce::dontSendNotification);

    titleLabel.setJustificationType(juce::Justification::centred);

    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(recordButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(saveLoopButton);
    addAndMakeVisible(loadLoopButton);
    addAndMakeVisible(loadWavButton);

    addAndMakeVisible(track1);
    addAndMakeVisible(track2);
    addAndMakeVisible(track3);
    addAndMakeVisible(track4);

    loopProgressBar.setPercentageDisplay(false);
    addAndMakeVisible(loopProgressBar);

    loopTimeLabel.setText("0:00", juce::dontSendNotification); // placeholder track time value
    loopTimeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(loopTimeLabel);

    // Save Loop Dialog box handler
    saveLoopButton.onClick = [this]() {
        juce::DialogWindow::LaunchOptions options;

        options.content.setOwned(
            new SaveLoopComponent()); // Create dialog window's contents with SaveLoopComponent
        options.dialogTitle = "Save Loop";
        options.dialogBackgroundColour = juce::Colour(0xff20242b);
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
        options.dialogBackgroundColour = juce::Colour(0xff20242b);
        options.componentToCentreAround = this;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    };

    setSize(1200, 650);

    // ----- Audio work required here -----
    setAudioChannels(0, 2);
    formatManager.registerBasicFormats();
    transportSource.addChangeListener(this);

    // Audio button 'clicked' functions defined in src/audio/MainComponentAudio.cpp
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setEnabled(false); // is enabled once user selects .wav
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.setEnabled(false);
    loadWavButton.onClick = [this] { loadWavButtonClicked(); };
}

MainComponent::~MainComponent() {
    shutdownAudio();
};

void MainComponent::paint(juce::Graphics& graphics) {
    // fill background
    graphics.fillAll(juce::Colour(0xff20242b));
}

// Called initially and when main components size changed.
void MainComponent::resized() {
    auto area = getLocalBounds().reduced(20); // 20px margin on sides

    // position label
    titleLabel.setBounds(area.removeFromTop(60)); // 60px title strip

    // position buttons
    auto buttonRow = area.removeFromTop(40); // 40 px button strip

    recordButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);

    playButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);

    stopButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);

    saveLoopButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);

    loadLoopButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);

    loadWavButton.setBounds(buttonRow.removeFromLeft(140));

    area.removeFromTop(20);

    // Position progress row containing progress bar and track progress time display.
    auto progressRow = area.removeFromTop(25);
    loopTimeLabel.setBounds(progressRow.removeFromRight(65));
    progressRow.removeFromRight(10);
    loopProgressBar.setBounds(progressRow);

    area.removeFromTop(10);

    // Position track rows
    track1.setBounds(area.removeFromTop(70));
    track2.setBounds(area.removeFromTop(70));
    track3.setBounds(area.removeFromTop(70));
    track4.setBounds(area.removeFromTop(70));
}