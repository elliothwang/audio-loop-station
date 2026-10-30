#pragma once

#include "MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Pass info to any other potential AudioSource's
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

// Tell other AudioSource's to stop
// (hs: I suspect I will use this when playing 4 sources concurrently)
void MainComponent::releaseResources() {
    transportSource.releaseResources();
}

// Audio callback function responsible for processing audio, called from another thread.
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {

    if (readerSource.get() == nullptr) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    transportSource.getNextAudioBlock(bufferToFill);
}

// Called by another thread when transport change is reported
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &transportSource) {
        if (transportSource.isPlaying())
            changeState(Playing);
        else
            changeState(Stopped);
    }
}

// Manage changing audio / transportSource states
void MainComponent::changeState(TransportState newState) {
    if (audioState != newState) {
        audioState = newState;
        switch (audioState) {
            case Stopped:
                stopButton.setEnabled(false);
                playButton.setEnabled(true);
                transportSource.setPosition(0.0);
                break;
            case Starting:
                playButton.setEnabled(false);
                transportSource.start();
                break;
            case Playing:
                stopButton.setEnabled(true);
                break;
            case Stopping:
                transportSource.stop();
                break;
        }
    }
}

// currently just a passthrough, might need expanded in future
void MainComponent::playButtonClicked() {
    changeState(Starting);
}

// currently just a passthrough, might need expanded in future
void MainComponent::stopButtonClicked() {
    changeState(Stopping);
}

// Opens a popup window that allows user to select .wav file
void MainComponent::loadWavButtonClicked() {
    // create file chooser object
    chooser =
        std::make_unique<juce::FileChooser>("Select a .wav file to play...", juce::File{}, "*.wav");
    auto chooserFlags =
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    // launch popup
    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();

        if (file != juce::File{}) {
            // create file reader object
            auto* reader = formatManager.createReaderFor(file);
            if (reader != nullptr) {
                // create new source
                auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
                playButton.setEnabled(true);
                readerSource.reset(newSource.release());
            }
        }
    });
}