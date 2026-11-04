#pragma once

// Colours and control drawing for the console lanes design. MainComponent installs it as the
// default look and feel, so every component, dialog, and tooltip picks it up.

#include <juce_gui_basics/juce_gui_basics.h>

// The design's colours, shared by the look and feel and by components that paint themselves.
namespace palette {
inline const juce::Colour background{0xff0f1218};
inline const juce::Colour header{0xff161a22};
inline const juce::Colour lane{0xff12151c};
inline const juce::Colour meterTrack{0xff0b0d12};
inline const juce::Colour border{0xff262c37};
inline const juce::Colour control{0xff1f2530};
inline const juce::Colour controlBorder{0xff323a47};
inline const juce::Colour text{0xffe4e8ef};
inline const juce::Colour mutedText{0xff9aa3b2};
inline const juce::Colour accent{0xff3dd6a8};
inline const juce::Colour record{0xffff5f57};
inline const juce::Colour mute{0xfff5c451};
inline const juce::Colour reverse{0xff62a8ff};
} // namespace palette

// Flat rounded buttons, a thin horizontal slider, and a pointer-style rotary knob.
class LoopStationLookAndFeel : public juce::LookAndFeel_V4 {
  public:
    // Sets the colour scheme and the colours of the stock components the app uses.
    LoopStationLookAndFeel();

    // Draws a rounded button with a one pixel border, lighter while hovered or pressed.
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour, bool highlighted,
                              bool down) override;

    // Draws horizontal sliders as a thin track with a round thumb. Other styles use the default.
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    // Draws a knob as a dark circle with a line pointing at the current value.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
};
