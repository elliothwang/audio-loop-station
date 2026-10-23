#include "LoadLoopComponent.h"

// LoadLoopComponent constructor
LoadLoopComponent::LoadLoopComponent() {
    headingLabel.setText("Previously saved loops", juce::dontSendNotification);

    loopNameHeader.setText("Loop name", juce::dontSendNotification);

    durationHeader.setText("Duration", juce::dontSendNotification);

    addAndMakeVisible(headingLabel);
    addAndMakeVisible(loopNameHeader);
    addAndMakeVisible(durationHeader);

    const std::array<juce::String, 5> names{"Practice Loop 1", "Bass Line", "Practice Loop 2",
                                            "Practice Loop 3", "Rhythm"};

    const std::array<juce::String, 5> durations{"0:08", "0:16", "0:12", "0:20", "0:04"};

    // Assign name and duration to each label pair
    for (std::size_t i = 0; i < loopNameLabels.size(); ++i) {
        loopNameLabels[i].setText(names[i], juce::dontSendNotification);

        durationLabels[i].setText(durations[i], juce::dontSendNotification);

        addAndMakeVisible(loopNameLabels[i]);
        addAndMakeVisible(durationLabels[i]);
    }

    addAndMakeVisible(loadButton);
    addAndMakeVisible(cancelButton);

    setSize(440, 320);
}

void LoadLoopComponent::resized() {
    auto area = getLocalBounds().reduced(20);

    headingLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    auto headerRow = area.removeFromTop(25);
    durationHeader.setBounds(headerRow.removeFromRight(90));
    loopNameHeader.setBounds(headerRow);

    // Each loop iteration creates 30px row, 90px "column" on right for duration.
    for (std::size_t i = 0; i < loopNameLabels.size(); ++i) {
        auto row = area.removeFromTop(30);

        durationLabels[i].setBounds(row.removeFromRight(90));
        loopNameLabels[i].setBounds(row);
    }

    area.removeFromTop(20);

    auto buttonRow = area.removeFromTop(30);

    loadButton.setBounds(buttonRow.removeFromRight(90));
    buttonRow.removeFromRight(10);
    cancelButton.setBounds(buttonRow.removeFromRight(90));
}