#include "LevelMeter.h"

#include "LoopStationLookAndFeel.h"

void LevelMeter::setLevel(float newLevel) {
    const float clamped = juce::jlimit(0.0f, 1.0f, newLevel);
    if (clamped != level) {
        level = clamped;
        repaint();
    }
}

void LevelMeter::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(palette::meterTrack);
    g.fillRoundedRectangle(bounds, 3.0f);
    if (level <= 0.0f) {
        return;
    }

    // The gradient spans the whole meter, so a bar only shows red once it reaches the top.
    juce::ColourGradient gradient(palette::accent, bounds.getBottomLeft(), palette::record,
                                  bounds.getTopLeft(), false);
    gradient.addColour(0.7, palette::accent);
    gradient.addColour(0.85, palette::mute);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds.withTop(bounds.getBottom() - bounds.getHeight() * level), 3.0f);
}
