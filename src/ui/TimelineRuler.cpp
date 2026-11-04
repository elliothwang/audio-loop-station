#include "TimelineRuler.h"

#include "LoopStationLookAndFeel.h"
#include "TrackLane.h"

#include <cmath>

namespace {

constexpr int kMaximumMarks = 16; // above this many seconds, marks are spaced further apart

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
