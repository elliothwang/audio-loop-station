#pragma once

#include "AudioRecorder.h"
#include "DeviceFooter.h"
#include "LoopStationLookAndFeel.h"
#include "Playhead.h"
#include "TimelineRuler.h"
#include "TrackLane.h"
#include "TransportBar.h"
#include "core/LoopPlayer.h"
#include "storage/LoopFile.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

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

    void timerCallback() override;

    // Feeds the time readout, ruler, and playhead from the playing loop.
    void updateTimeline();

    // Saves the recorded clip as a loop called `name`, and reports how it went.
    void saveLoop(const juce::String& name);

    TimelineRuler timelineRuler;
    TrackLane track1{1};
    TrackLane track2{2};
    TrackLane track3{3};
    TrackLane track4{4};
    Playhead playhead; // after the lanes, so it draws over them
    DeviceFooter deviceFooter{deviceManager};

    // ----- Audio, needs declared here -----
    AudioRecorder audioRecorder;
    als::LoopPlayer loopPlayer; // plays the recorded clip

    void playButtonClicked();
    void stopButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};