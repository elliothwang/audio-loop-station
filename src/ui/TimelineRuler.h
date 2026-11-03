#pragma once

// The time ruler above the lanes. It skips the lanes' header width, so its marks line up with the
// waveform areas.

#include <juce_gui_basics/juce_gui_basics.h>

// Second marks across the waveform width. It stays blank until there is a length to show.
class TimelineRuler : public juce::Component {
  public:
    // Sets the length the ruler spans, in seconds, and repaints only if it changed.
    void setLength(double lengthSeconds);

    // Draws the background, the header caption, and one mark per step.
    void paint(juce::Graphics& g) override;

  private:
    double length = 0.0; // [s]
};
