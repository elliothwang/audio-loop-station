#pragma once

#include "TrackRowComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Declare main UI component, GUI behaviour in MainComponent.cpp
class MainComponent : public juce::AudioAppComponent, public juce::ChangeListener {
  public:
    MainComponent();           // Declare constructor to configure component
    ~MainComponent() override; // Destructor

    void paint(juce::Graphics&) override; // background, custom graphics
    void resized() override;              // position controls

    // AudioAppComponent functions defined in src/audio/MainComponentAudio.cpp
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override; // Audio Callback
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

  private:
    juce::Label titleLabel; // text label for component
    juce::TextButton recordButton{"Record"};
    juce::TextButton playButton{"Play"};
    juce::TextButton stopButton{"Stop"};
    juce::TextButton saveLoopButton{"Save Loop"};
    juce::TextButton loadLoopButton{"Load Loop"};
    juce::TextButton loadWavButton{"Select .wav"}; // single wav until full loop implementation

    // progress bar declarations
    double loopProgress = 0.60; // placeholder value for progress bar
    juce::ProgressBar loopProgressBar{loopProgress};
    juce::Label loopTimeLabel;

    TrackRowComponent track1{1};
    TrackRowComponent track2{2};
    TrackRowComponent track3{3};
    TrackRowComponent track4{4};

    // ----- Audio, needs declared here -----
    enum TransportState { Stopped, Starting, Playing, Stopping };

    /*
    See: https://juce.com/tutorials/tutorial_playing_sound_files/
    for more info about these
    */
    juce::AudioFormatManager formatManager;                      // creates objects that read audio
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource; // reads audio
    juce::AudioTransportSource transportSource;                  // controls playback
    TransportState audioState;                  // represents state of transportSource
    std::unique_ptr<juce::FileChooser> chooser; // object that chooses .wav file

    void changeState(TransportState newState);
    void playButtonClicked();
    void stopButtonClicked();
    void loadWavButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};