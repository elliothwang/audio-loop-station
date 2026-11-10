#pragma once

// The saved loops panel beside the lanes. It lists the saved loops and hands the chosen one's
// folder to MainComponent to load.

#include "storage/LoopFile.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Declare LoadLoopComponent, behaviour in LoadLoopComponent.cpp
class LoadLoopComponent : public juce::Component, private juce::ListBoxModel {
  public:
    // Starts with no loops listed. Load, or double-clicking a row, passes the chosen loop's folder
    // to `onLoad`.
    explicit LoadLoopComponent(std::function<void(const juce::File&)> onLoad);

    // Lists `loops` in place of the ones shown before, with nothing selected.
    void setLoops(std::vector<als::LoopInfo> loops);

    // Fills the panel and draws the divider along its left edge.
    void paint(juce::Graphics& graphics) override;

    // Lays out the heading, column headers, list, and Load button.
    void resized() override;

  private:
    // Returns how many loops are listed.
    int getNumRows() override;

    // Draws one loop's name and duration.
    void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height,
                          bool selected) override;

    // Loads the double-clicked loop.
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    // Enables Load once a loop is selected.
    void selectedRowsChanged(int lastRowSelected) override;

    // Passes `row`'s folder to onLoad.
    void load(int row);

    std::vector<als::LoopInfo> loops;
    std::function<void(const juce::File&)> onLoad;

    juce::Label headingLabel;
    juce::Label loopNameHeader;
    juce::Label durationHeader;
    juce::ListBox loopList;

    juce::TextButton loadButton{"Load"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoadLoopComponent)
};
