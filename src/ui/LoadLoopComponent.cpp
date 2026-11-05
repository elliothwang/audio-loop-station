#include "LoadLoopComponent.h"

#include "LoopStationLookAndFeel.h"

#include <utility>

namespace {

constexpr int kDurationWidth = 90; // [px], the right-hand column

// Formats a loop's length in minutes and whole seconds, as the dialog's duration column.
juce::String formatDuration(const als::LoopInfo& loop) {
    const int seconds = juce::roundToInt(static_cast<double>(loop.length) / loop.sampleRate);
    return juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
}

} // namespace

// LoadLoopComponent constructor
LoadLoopComponent::LoadLoopComponent(std::vector<als::LoopInfo> savedLoops,
                                     std::function<void(const juce::File&)> loadCallback)
    : loops(std::move(savedLoops)), onLoad(std::move(loadCallback)) {
    headingLabel.setText(loops.empty() ? "No saved loops yet. Save a loop to see it here."
                                       : "Previously saved loops",
                         juce::dontSendNotification);

    loopNameHeader.setText("Loop name", juce::dontSendNotification);

    durationHeader.setText("Duration", juce::dontSendNotification);

    loopList.setModel(this);
    loopList.setRowHeight(30);
    loopList.setColour(juce::ListBox::backgroundColourId, palette::lane);

    loadButton.setEnabled(false); // until a loop is selected
    loadButton.onClick = [this]() { load(loopList.getSelectedRow()); };
    cancelButton.onClick = [this]() { close(); };

    addAndMakeVisible(headingLabel);
    addAndMakeVisible(loopNameHeader);
    addAndMakeVisible(durationHeader);
    addAndMakeVisible(loopList);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(cancelButton);

    setSize(440, 320);
}

void LoadLoopComponent::resized() {
    auto area = getLocalBounds().reduced(20);

    headingLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    auto headerRow = area.removeFromTop(25);
    durationHeader.setBounds(headerRow.removeFromRight(kDurationWidth));
    loopNameHeader.setBounds(headerRow);

    auto buttonRow = area.removeFromBottom(30);
    area.removeFromBottom(20);

    loadButton.setBounds(buttonRow.removeFromRight(90));
    buttonRow.removeFromRight(10);
    cancelButton.setBounds(buttonRow.removeFromRight(90));

    loopList.setBounds(area);
}

int LoadLoopComponent::getNumRows() {
    return static_cast<int>(loops.size());
}

void LoadLoopComponent::paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                                         bool selected) {
    if (row < 0 || row >= getNumRows()) {
        return;
    }
    if (selected) {
        g.fillAll(palette::control);
    }
    const als::LoopInfo& loop = loops[static_cast<std::size_t>(row)];
    // Inset to line up with the column headers, whose labels pad their text.
    auto bounds = juce::Rectangle<int>(width, height).reduced(4, 0);
    g.setColour(palette::text);
    g.setFont(14.0f);
    g.drawText(formatDuration(loop), bounds.removeFromRight(kDurationWidth - 4),
               juce::Justification::centredLeft);
    g.drawText(loop.name, bounds, juce::Justification::centredLeft, true);
}

void LoadLoopComponent::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) {
    load(row);
}

void LoadLoopComponent::selectedRowsChanged(int lastRowSelected) {
    loadButton.setEnabled(lastRowSelected >= 0);
}

void LoadLoopComponent::load(int row) {
    if (row < 0 || row >= getNumRows()) {
        return;
    }
    // Closing deletes this component, so the load runs first.
    onLoad(loops[static_cast<std::size_t>(row)].folder);
    close();
}

void LoadLoopComponent::close() {
    // The dialog deletes itself, and this component with it, once it leaves the modal state.
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) {
        dialog->exitModalState(0);
    }
}
