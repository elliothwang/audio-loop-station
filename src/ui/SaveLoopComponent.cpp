#include "SaveLoopComponent.h"

// Constructor, sets label and makes visible input controls, initial dialog sizing
SaveLoopComponent::SaveLoopComponent() {
    loopNameLabel.setText("Loop name", juce::dontSendNotification);

    addAndMakeVisible(loopNameLabel);
    addAndMakeVisible(loopNameEditor);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(cancelButton);

    setSize(400, 160);
}

void SaveLoopComponent::resized() {
    auto area = getLocalBounds().reduced(20);

    loopNameLabel.setBounds(area.removeFromTop(25));
    loopNameEditor.setBounds(area.removeFromTop(30));

    area.removeFromTop(20);

    auto buttonRow = area.removeFromTop(30);

    saveButton.setBounds(buttonRow.removeFromRight(90));
    buttonRow.removeFromRight(10);
    cancelButton.setBounds(buttonRow.removeFromRight(90));
}