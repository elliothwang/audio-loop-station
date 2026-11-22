#include "IconButton.h"

#include "LoopStationLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPulsePeriodMs = 1200.0;
constexpr int kGearTeeth = 8;

// Opacity for a toggled-on fill, cycling slowly so a live take is hard to miss.
float pulseAlpha() {
    const double phase =
        std::fmod(juce::Time::getMillisecondCounterHiRes(), kPulsePeriodMs) / kPulsePeriodMs;
    return 0.7f + 0.3f * static_cast<float>(std::sin(phase * juce::MathConstants<double>::twoPi));
}

} // namespace

IconButton::IconButton(const juce::String& name, Icon iconToDraw, juce::Colour iconColour,
                       Style buttonStyle)
    : juce::Button(name), icon(iconToDraw), colour(iconColour), style(buttonStyle) {
    setTooltip(name);
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    auto bounds = getLocalBounds().toFloat().reduced(1.5f);
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    const bool on = getToggleState();
    const bool round = style == Style::Round;

    if (style == Style::Boxed) {
        // The look and feel's own text button background, so it matches Save and Load exactly.
        getLookAndFeel().drawButtonBackground(
            g, *this, findColour(juce::TextButton::buttonColourId), highlighted, down);
    } else if (round) {
        const float size = std::min(bounds.getWidth(), bounds.getHeight());
        bounds = bounds.withSizeKeepingCentre(size, size);
        auto fill = on ? colour.withAlpha(pulseAlpha()) : palette::control;
        if (down) {
            fill = fill.brighter(0.15f);
        } else if (highlighted) {
            fill = fill.brighter(0.08f);
        }
        g.setColour(fill.withMultipliedAlpha(alpha));
        g.fillEllipse(bounds);
        g.setColour(colour.withMultipliedAlpha(alpha));
        g.drawEllipse(bounds, 2.0f);
    } else if (highlighted || down) {
        g.setColour(palette::control.withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(bounds, 5.0f);
    }

    // A filled record button needs a dark icon to stay visible against its own colour.
    g.setColour((on && round ? palette::background : colour).withMultipliedAlpha(alpha));
    const float iconScale = icon == Icon::Delete ? 0.55f : icon == Icon::Settings ? 0.45f : 0.36f;
    paintIcon(g, bounds.withSizeKeepingCentre(bounds.getWidth() * iconScale,
                                              bounds.getHeight() * iconScale));
}

void IconButton::paintIcon(juce::Graphics& g, juce::Rectangle<float> area) const {
    switch (icon) {
    case Icon::Record:
        g.fillEllipse(area);
        break;
    case Icon::Play: {
        juce::Path triangle;
        // Nudged right because a triangle's visual centre sits right of its bounding box centre.
        const float nudge = area.getWidth() * 0.1f;
        triangle.addTriangle(area.getX() + nudge, area.getY(), area.getX() + nudge,
                             area.getBottom(), area.getRight() + nudge, area.getCentreY());
        g.fillPath(triangle);
        break;
    }
    case Icon::Stop:
        g.fillRoundedRectangle(area, 2.0f);
        break;
    case Icon::Delete: {
        // A bin, drawn in a 14 by 14 box and scaled to fit.
        juce::Path bin;
        bin.startNewSubPath(2.0f, 3.5f);
        bin.lineTo(12.0f, 3.5f);
        bin.startNewSubPath(5.5f, 3.5f);
        bin.lineTo(5.5f, 2.0f);
        bin.lineTo(8.5f, 2.0f);
        bin.lineTo(8.5f, 3.5f);
        bin.startNewSubPath(3.5f, 3.5f);
        bin.lineTo(4.2f, 12.0f);
        bin.lineTo(9.8f, 12.0f);
        bin.lineTo(10.5f, 3.5f);
        bin.applyTransform(bin.getTransformToScaleToFit(area, true));
        g.strokePath(bin, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        break;
    }
    case Icon::Settings: {
        // A gear, drawn in a 14 by 14 box and scaled to fit. The ring and teeth are separate
        // strokes, so the teeth can be wider and square ended.
        juce::Path ring;
        ring.addEllipse(3.5f, 3.5f, 7.0f, 7.0f);
        juce::Path teeth;
        for (int tooth = 0; tooth < kGearTeeth; ++tooth) {
            const float angle = juce::MathConstants<float>::twoPi * static_cast<float>(tooth) /
                                static_cast<float>(kGearTeeth);
            teeth.startNewSubPath(7.0f + 3.5f * std::sin(angle), 7.0f - 3.5f * std::cos(angle));
            teeth.lineTo(7.0f + 6.0f * std::sin(angle), 7.0f - 6.0f * std::cos(angle));
        }
        const auto fit = teeth.getTransformToScaleToFit(area, true);
        ring.applyTransform(fit);
        teeth.applyTransform(fit);
        g.strokePath(ring, juce::PathStrokeType(1.8f));
        g.strokePath(teeth, juce::PathStrokeType(2.4f, juce::PathStrokeType::mitered,
                                                 juce::PathStrokeType::butt));
        break;
    }
    }
}
