#pragma once

#include "TrackRowComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

// Declare main UI component, GUI behaviour in MainComponent.cpp
class MainComponent : public juce::AudioAppComponent {
  public:
    MainComponent(); // Declare constructor to configure component
    ~MainComponent() override; // Destructor

    void paint(juce::Graphics&) override; // background, custom graphics
    void resized() override;              // position controls

    // AudioAppComponent functions defined in src/audio/MainComponentAudio.cpp
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;        // logging
    void releaseResources() override;                                                   // logging
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;  // processing

  private:
    juce::Label titleLabel; // text label for component
    juce::TextButton recordButton{"Record"};
    juce::TextButton playStopButton{"Play / Stop"};
    juce::TextButton saveLoopButton{"Save Loop"};
    juce::TextButton loadLoopButton{"Load Loop"};

    // progress bar declarations
    double loopProgress = 0.60; // placeholder value for progress bar
    juce::ProgressBar loopProgressBar{loopProgress};
    juce::Label loopTimeLabel;

    TrackRowComponent track1{1};
    TrackRowComponent track2{2};
    TrackRowComponent track3{3};
    TrackRowComponent track4{4};

    juce::Random random; // TODO: remove Creates random values for generating white noise

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};