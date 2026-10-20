#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

// Declare LoadLoopComponent, behaviour in LoadLoopComponent.cpp
class LoadLoopComponent : public juce::Component {
  public:
    LoadLoopComponent();

    void resized() override;

  private:
    juce::Label headingLabel;
    juce::Label loopNameHeader;
    juce::Label durationHeader;

    // Placeholder fixed data of 5 saved loops. To update.
    std::array<juce::Label, 5> loopNameLabels;
    std::array<juce::Label, 5> durationLabels;

    juce::TextButton loadButton{"Load"};
    juce::TextButton cancelButton{"Cancel"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoadLoopComponent)
};