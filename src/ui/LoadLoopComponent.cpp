#include "LoadLoopComponent.h"

#include "LoopStationLookAndFeel.h"

#include <utility>

namespace {

constexpr int kDurationWidth = 70; // [px], the right-hand column

// Formats a loop's length in minutes and whole seconds, as the panel's duration column.
juce::String formatDuration(const als::LoopInfo& loop) {
    const int seconds = juce::roundToInt(static_cast<double>(loop.length) / loop.sampleRate);
    return juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
}

} // namespace

// LoadLoopComponent constructor
LoadLoopComponent::LoadLoopComponent(std::function<void(const juce::File&)> loadCallback)
    : onLoad(std::move(loadCallback)) {
    headingLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

    loopNameHeader.setText("Loop name", juce::dontSendNotification);
    loopNameHeader.setColour(juce::Label::textColourId, palette::mutedText);

    durationHeader.setText("Duration", juce::dontSendNotification);
    durationHeader.setColour(juce::Label::textColourId, palette::mutedText);

    loopList.setModel(this);
    loopList.setRowHeight(30);
    loopList.setColour(juce::ListBox::backgroundColourId, palette::lane);

    loadButton.onClick = [this]() { load(loopList.getSelectedRow()); };

    addAndMakeVisible(headingLabel);
    addAndMakeVisible(loopNameHeader);
    addAndMakeVisible(durationHeader);
    addAndMakeVisible(loopList);
    addAndMakeVisible(loadButton);

    setLoops({});
}

void LoadLoopComponent::setLoops(std::vector<als::LoopInfo> savedLoops) {
    loops = std::move(savedLoops);
    headingLabel.setText(loops.empty() ? "No saved loops yet" : "Saved loops",
                         juce::dontSendNotification);
    loopList.updateContent();
    loopList.deselectAllRows();
    loadButton.setEnabled(false); // until a loop is selected
    repaint();
}

void LoadLoopComponent::paint(juce::Graphics& graphics) {
    graphics.fillAll(palette::header);
    graphics.setColour(palette::border);
    graphics.fillRect(0, 0, 1, getHeight());
}

void LoadLoopComponent::resized() {
    auto area = getLocalBounds().reduced(12);

    headingLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);

    auto headerRow = area.removeFromTop(22);
    durationHeader.setBounds(headerRow.removeFromRight(kDurationWidth));
    loopNameHeader.setBounds(headerRow);

    auto buttonRow = area.removeFromBottom(30);
    area.removeFromBottom(10);
    loadButton.setBounds(buttonRow.removeFromRight(90));

    loopList.setBounds(area);
}

int LoadLoopComponent::getNumRows() {
    return static_cast<int>(loops.size());
}

void LoadLoopComponent::paintListBoxItem(int row, juce::Graphics& graphics, int width, int height,
                                         bool selected) {
    if (row < 0 || row >= getNumRows()) {
        return;
    }
    if (selected) {
        graphics.fillAll(palette::control);
    }
    const als::LoopInfo& loop = loops[static_cast<std::size_t>(row)];
    // Inset to line up with the column headers, whose labels pad their text.
    auto bounds = juce::Rectangle<int>(width, height).reduced(4, 0);
    graphics.setColour(palette::text);
    graphics.setFont(14.0f);
    graphics.drawText(formatDuration(loop), bounds.removeFromRight(kDurationWidth - 4),
                      juce::Justification::centredLeft);
    graphics.drawText(loop.name, bounds, juce::Justification::centredLeft, true);
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
    onLoad(loops[static_cast<std::size_t>(row)].folder);
}
