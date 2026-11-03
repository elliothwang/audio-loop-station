#pragma once

#include "AudioRecorder.h"
#include "DeviceFooter.h"
#include "LoopStationLookAndFeel.h"
#include "TimelineRuler.h"
#include "TrackLane.h"
#include "TransportBar.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Declare main UI component, GUI behaviour in MainComponent.cpp
class MainComponent : public juce::AudioAppComponent,
                      public juce::ChangeListener,
                      private juce::Timer {
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
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

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

    juce::TextButton loadWavButton{"Select .wav"}; // single wav until full loop implementation
    juce::TextButton saveClipButton{"Save Clip"};
    void timerCallback() override;

    // Feeds the time readout, ruler, and playhead from the selected .wav until loops play.
    void updateTimeline();

    TimelineRuler timelineRuler;
    TrackLane track1{1};
    TrackLane track2{2};
    TrackLane track3{3};
    TrackLane track4{4};
    Playhead playhead; // after the lanes, so it draws over them
    DeviceFooter deviceFooter{deviceManager};

    // ----- Audio, needs declared here -----
    AudioRecorder audioRecorder;
    enum TransportState { Stopped, Starting, Playing, Stopping };

    /*
    See: https://juce.com/tutorials/tutorial_playing_sound_files/
    for more info about these
    */
    juce::AudioFormatManager formatManager;                      // creates objects that read audio
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource; // reads audio
    juce::AudioTransportSource transportSource;                  // controls playback
    TransportState audioState;                  // represents state of transportSource
    std::unique_ptr<juce::FileChooser> chooser; // object that chooses .wav file to play
    std::unique_ptr<juce::FileChooser>
        saveClipChooser; // object to choose where to save a recording

    void changeState(TransportState newState);
    void playButtonClicked();
    void stopButtonClicked();
    void loadWavButtonClicked();
    void saveClipButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};