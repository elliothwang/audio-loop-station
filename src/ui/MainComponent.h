#pragma once

#include "TrackRowComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Declare main UI component, behaviour in MainComponent.cpp
class MainComponent : public juce::Component {
  public:
    MainComponent(); // Declare constructor to configure component

    void paint(juce::Graphics&) override; // background, custom graphics
    void resized() override;              // position controls

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};