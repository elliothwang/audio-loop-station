#pragma once

// A button drawn as an icon instead of text, used by the transport and the track lanes.

#include <juce_gui_basics/juce_gui_basics.h>

// Paints one of a few vector icons. While toggled on, a round button fills with its colour and
// pulses, which is how the record button shows that a take is in progress.
class IconButton : public juce::Button {
  public:
    enum class Icon { Record, Play, Stop, Delete, Settings };

    // Round has a ring, like the transport. Flat only shows a background while hovered, like a
    // lane's delete. Boxed is drawn like a text button, like Save and Load.
    enum class Style { Round, Flat, Boxed };

    // `name` is the accessible title and the tooltip. `colour` tints the icon and, for a round
    // button, its ring.
    IconButton(const juce::String& name, Icon icon, juce::Colour colour, Style style);

    // Draws the background, ring, and icon for the current state.
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    // Draws the icon shape inside `area`.
    void paintIcon(juce::Graphics& g, juce::Rectangle<float> area) const;

    Icon icon;
    juce::Colour colour;
    Style style;
};
