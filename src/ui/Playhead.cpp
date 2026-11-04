#include "Playhead.h"

#include "LoopStationLookAndFeel.h"
#include "TrackLane.h"

namespace {

constexpr int kPlayheadWidth = 2; // [px]
constexpr int kRepaintMargin = 2; // [px] each side of the line, so no trace is left behind

} // namespace

Playhead::Playhead() {
    setInterceptsMouseClicks(false, false);
}

void Playhead::setFraction(double newFraction) {
    if (newFraction == fraction) {
        return;
    }
    // Repaint only the strips the line leaves and enters, so the lanes underneath don't redraw.
    if (fraction >= 0.0) {
        repaint(xFor(fraction) - kRepaintMargin, 0, kPlayheadWidth + 2 * kRepaintMargin,
                getHeight());
    }
    fraction = newFraction;
    if (fraction >= 0.0) {
        repaint(xFor(fraction) - kRepaintMargin, 0, kPlayheadWidth + 2 * kRepaintMargin,
                getHeight());
    }
}

void Playhead::paint(juce::Graphics& g) {
    if (fraction < 0.0) {
        return;
    }
    g.setColour(palette::text.withAlpha(0.85f));
    g.fillRect(xFor(fraction) - kPlayheadWidth / 2, 0, kPlayheadWidth, getHeight());
}

int Playhead::xFor(double positionFraction) const {
    const double width = getWidth() - TrackLane::kHeaderWidth;
    return TrackLane::kHeaderWidth +
           juce::roundToInt(juce::jlimit(0.0, 1.0, positionFraction) * width);
}
