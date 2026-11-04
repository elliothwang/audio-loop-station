#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class SaveLoopComponent : public juce::Component // SaveLoop inherits from juce::Component
{
  public:
    // Save, or Return in the name field, passes the typed name to `onSave` and closes the dialog.
    explicit SaveLoopComponent(std::function<void(const juce::String&)> onSave);

    void resized() override;

  private:
    // Closes the dialog window this component sits in.
    void close();

    juce::Label loopNameLabel;
    juce::TextEditor loopNameEditor; // Field for user input

    juce::TextButton saveButton{"Save"};
    juce::TextButton cancelButton{"Cancel"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveLoopComponent)
};
