#pragma once

// The Load dialog's contents. It lists the saved loops and hands the chosen one's folder to
// MainComponent to load.

#include "storage/LoopFile.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Declare LoadLoopComponent, behaviour in LoadLoopComponent.cpp
class LoadLoopComponent : public juce::Component, private juce::ListBoxModel {
  public:
    // Lists `loops`. Load, or double-clicking a row, passes the chosen loop's folder to `onLoad`
    // and closes the dialog.
    LoadLoopComponent(std::vector<als::LoopInfo> loops,
                      std::function<void(const juce::File&)> onLoad);

    void resized() override;

  private:
    // Returns how many loops are listed.
    int getNumRows() override;

    // Draws one loop's name and duration.
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                          bool selected) override;

    // Loads the double-clicked loop.
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    // Enables Load once a loop is selected.
    void selectedRowsChanged(int lastRowSelected) override;

    // Passes `row`'s folder to onLoad and closes the dialog.
    void load(int row);

    // Closes the dialog window this component sits in.
    void close();

    std::vector<als::LoopInfo> loops;
    std::function<void(const juce::File&)> onLoad;

    juce::Label headingLabel;
    juce::Label loopNameHeader;
    juce::Label durationHeader;
    juce::ListBox loopList;

    juce::TextButton loadButton{"Load"};
    juce::TextButton cancelButton{"Cancel"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoadLoopComponent)
};
