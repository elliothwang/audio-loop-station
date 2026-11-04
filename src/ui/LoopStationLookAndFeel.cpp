#include "LoopStationLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kCornerRadius = 5.0f;
constexpr float kTrackThickness = 4.0f;
constexpr float kThumbDiameter = 14.0f;

} // namespace

LoopStationLookAndFeel::LoopStationLookAndFeel() {
    setColourScheme({palette::background, palette::header, palette::header, palette::controlBorder,
                     palette::text, palette::accent, palette::background, palette::accent,
                     palette::text});

    setColour(juce::ResizableWindow::backgroundColourId, palette::background);
    setColour(juce::TextButton::buttonColourId, palette::control);
    setColour(juce::TextButton::textColourOffId, palette::text);
    setColour(juce::TextButton::textColourOnId, palette::background);
    setColour(juce::Label::textColourId, palette::text);
    setColour(juce::Slider::trackColourId, palette::accent);
    setColour(juce::Slider::textBoxTextColourId, palette::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::backgroundColourId, palette::control);
    setColour(juce::TextEditor::textColourId, palette::text);
    setColour(juce::TextEditor::outlineColourId, palette::controlBorder);
    setColour(juce::TextEditor::focusedOutlineColourId, palette::accent);
    setColour(juce::TooltipWindow::backgroundColourId, palette::header);
    setColour(juce::TooltipWindow::textColourId, palette::text);
    setColour(juce::TooltipWindow::outlineColourId, palette::border);
}

void LoopStationLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool highlighted, bool down) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = backgroundColour;
    if (down) {
        fill = fill.brighter(0.15f);
    } else if (highlighted) {
        fill = fill.brighter(0.08f);
    }
    // Disabled buttons fade rather than change colour, so their meaning stays readable.
    const float alpha = button.isEnabled() ? 1.0f : 0.45f;
    g.setColour(fill.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(palette::controlBorder.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.0f);
}

void LoopStationLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width,
                                              int height, float sliderPos, float minSliderPos,
                                              float maxSliderPos, juce::Slider::SliderStyle style,
                                              juce::Slider& slider) {
    if (!slider.isHorizontal()) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos,
                                         maxSliderPos, style, slider);
        return;
    }
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const juce::Rectangle<float> track(static_cast<float>(x), centreY - kTrackThickness * 0.5f,
                                       static_cast<float>(width), kTrackThickness);
    g.setColour(palette::border);
    g.fillRoundedRectangle(track, kTrackThickness * 0.5f);
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(track.withRight(sliderPos), kTrackThickness * 0.5f);
    g.setColour(palette::text);
    g.fillEllipse(
        juce::Rectangle<float>(kThumbDiameter, kThumbDiameter).withCentre({sliderPos, centreY}));
}

void LoopStationLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width,
                                              int height, float sliderPosProportional,
                                              float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider&) {
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto circle = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
    g.setColour(palette::background);
    g.fillEllipse(circle);
    g.setColour(palette::controlBorder);
    g.drawEllipse(circle, 2.0f);

    // JUCE measures angles clockwise from twelve o'clock.
    const float angle =
        rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const juce::Point<float> tip =
        centre + juce::Point<float>(std::sin(angle), -std::cos(angle)) * (radius * 0.7f);
    g.setColour(palette::text);
    g.drawLine({centre, tip}, 3.0f);
}
