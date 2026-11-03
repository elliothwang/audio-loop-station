#pragma once

// The vertical level meter in each track lane. It shows silence until the engine reports levels.

#include <juce_gui_basics/juce_gui_basics.h>

// Draws a level from 0 to 1 as a bar that turns yellow, then red, near full scale.
class LevelMeter : public juce::Component {
  public:
    // Sets the level to show, clamped to 0 to 1, and repaints only if it changed.
    void setLevel(float newLevel);

    // Draws the empty track and the filled part of the bar.
    void paint(juce::Graphics& g) override;

  private:
    float level = 0.0f;
};
