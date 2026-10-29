#pragma once

#include "MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Provided in tutorial, mostly for logging info
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    juce::String message;
    message << "Preparing to play audio...\n";
    message << " samplesPerBlockExpected = " << samplesPerBlockExpected << "\n";
    message << " sampleRate = " << sampleRate;
    juce::Logger::getCurrentLogger()->writeToLog (message);
}

// Provided in tutorial, mostly for logging info
void MainComponent::releaseResources() {
    juce::Logger::getCurrentLogger()->writeToLog ("Releasing audio resources");
}

// Audio callback function responsible for processing audio, called from another thread.
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    // Currently just generating white noise while following tutorials...

    for (auto channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel) {
        // Get a pointer to the start sample in the buffer for this audio output channel
        auto* buffer = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample);
        // Fill the required number of samples with noise between -0.125 and +0.125
        for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            buffer[sample] = random.nextFloat() * 0.5f - 0.125f;
    }
}