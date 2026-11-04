#include "SaveLoopComponent.h"

// Constructor, sets label and makes visible input controls, initial dialog sizing
SaveLoopComponent::SaveLoopComponent(std::function<void(const juce::String&)> onSave) {
    loopNameLabel.setText("Loop name", juce::dontSendNotification);

    // Closing deletes this component and the callback with it, so the save runs first.
    auto save = [this, onSave = std::move(onSave)]() {
        onSave(loopNameEditor.getText());
        close();
    };
    saveButton.onClick = save;
    loopNameEditor.onReturnKey = save;
    cancelButton.onClick = [this]() { close(); };

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

void SaveLoopComponent::close() {
    // The dialog deletes itself, and this component with it, once it leaves the modal state.
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) {
        dialog->exitModalState(0);
    }
}
