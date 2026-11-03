#pragma once

// The time ruler above the lanes and the playhead line drawn across them. Both skip the lanes'
// header width, so their positions line up with the waveform areas.

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
