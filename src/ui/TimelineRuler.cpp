#include "TimelineRuler.h"

#include "LoopStationLookAndFeel.h"
#include "TrackLane.h"

#include <cmath>

namespace {

constexpr int kMaximumMarks = 16; // above this many seconds, marks are spaced further apart
constexpr int kPlayheadWidth = 2; // [px]
constexpr int kRepaintMargin = 2; // [px] each side of the line, so no trace is left behind

} // namespace

void TimelineRuler::setLength(double lengthSeconds) {
    if (lengthSeconds != length) {
        length = lengthSeconds;
        repaint();
    }
}

void TimelineRuler::paint(juce::Graphics& g) {
    g.fillAll(palette::lane);
    g.setColour(palette::border);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    g.setColour(palette::mutedText);
    g.setFont(11.0f);
    g.drawText("TRACKS", getLocalBounds().withWidth(TrackLane::kHeaderWidth).withTrimmedLeft(18),
               juce::Justification::centredLeft);
    if (length <= 0.0) {
        return;
    }

    // One mark per second for short loops, and at most kMaximumMarks marks for long ones.
    const double step = length <= kMaximumMarks ? 1.0 : std::ceil(length / kMaximumMarks);
    const double width = getWidth() - TrackLane::kHeaderWidth;
    const int markCount = static_cast<int>(std::ceil(length / step));
    for (int mark = 0; mark < markCount; ++mark) {
        const double seconds = mark * step;
        const int x = TrackLane::kHeaderWidth + juce::roundToInt(seconds / length * width);
        g.setColour(palette::border);
        g.fillRect(x, 0, 1, getHeight());
        g.setColour(palette::mutedText);
        g.drawText(juce::String(juce::roundToInt(seconds)) + "s", x + 6, 0, 40, getHeight(),
                   juce::Justification::centredLeft);
    }
}

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
