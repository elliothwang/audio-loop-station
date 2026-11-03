#pragma once

// The playhead line drawn across the ruler and lanes. It skips the lanes' header width, so its
// position lines up with the waveform areas.

#include <juce_gui_basics/juce_gui_basics.h>

// A vertical line at the playback position. It ignores the mouse so the lanes under it still work.
class Playhead : public juce::Component {
  public:
    // Lets clicks pass through to the components underneath.
    Playhead();

    // Moves the line to `newFraction` of the way across the waveform area. Negative hides it.
    void setFraction(double newFraction);

    // Draws the line.
    void paint(juce::Graphics& g) override;

  private:
    // Returns the line's x position for a fraction of the waveform width.
    int xFor(double positionFraction) const;

    double fraction = -1.0;
};
