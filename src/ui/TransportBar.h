#pragma once

// The header across the top of the window. It shows the loop's name and time and holds the
// transport and file buttons, which MainComponent wires up.

#include "IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Record, play, and stop as icons, the time readout, and Save and Load.
class TransportBar : public juce::Component {
  public:
    // Builds the labels and buttons with their placeholder text.
    TransportBar();

    // Shows the loop's name under the app's name.
    void setLoopName(const juce::String& name);

    // Shows a position and a length, both in seconds, in the time readout.
    void setTime(double positionSeconds, double lengthSeconds);

    // Draws the header background and its bottom border.
    void paint(juce::Graphics& g) override;

    // Positions the labels and buttons from left to right.
    void resized() override;

    // Returns the record button. MainComponent wires its clicks and sets its state.
    [[nodiscard]] juce::Button& recordButton() noexcept;

    // Returns the play button.
    [[nodiscard]] juce::Button& playButton() noexcept;

    // Returns the stop button.
    [[nodiscard]] juce::Button& stopButton() noexcept;

    // Returns the Save button.
    [[nodiscard]] juce::Button& saveLoopButton() noexcept;

    // Returns the Load button.
    [[nodiscard]] juce::Button& loadLoopButton() noexcept;

  private:
    juce::Label appLabel;
    juce::Label loopNameLabel;
    juce::Label timeLabel;
    IconButton record;
    IconButton play;
    IconButton stop;
    juce::TextButton saveLoop{"Save"};
    juce::TextButton loadLoop{"Load"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
