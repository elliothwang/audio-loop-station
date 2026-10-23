#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SaveLoopComponent : public juce::Component // SaveLoop inherits from juce::Component
{
  public:
    SaveLoopComponent(); // Declare constructor

    void resized() override;

  private:
    juce::Label loopNameLabel;
    juce::TextEditor loopNameEditor; // Field for user input

    juce::TextButton saveButton{"Save"};
    juce::TextButton cancelButton{"Cancel"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveLoopComponent)
};