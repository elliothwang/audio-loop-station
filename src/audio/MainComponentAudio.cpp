#pragma once

#include "MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Pass sample rate to recorder and player and prepare buffers.
void MainComponent::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) {
    audioRecorder.prepare(sampleRate);
    loopPlayer.prepare(sampleRate);
}

// Nothing to release, since the recorder and player keep their buffers between devices.
void MainComponent::releaseResources() {}

// Audio callback function responsible for processing audio, called repeatedly by JUCE on audio
// thread. Each call handles small audio block.
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    // Check that accessing buffer will be valid
    juce::AudioBuffer<float>* buffer = bufferToFill.buffer;
    if (buffer == nullptr || buffer->getNumChannels() == 0 || bufferToFill.numSamples <= 0) {
        return;
    }

    // Capture microphone input if recording active and copy into audioRecorder buffer using
    // processInput
    audioRecorder.processInput(buffer->getReadPointer(0, bufferToFill.startSample),
                               bufferToFill.numSamples);

    // The input shares this buffer, so clear it before writing output. Prevents microphone input
    // going direct through to output speakers.
    bufferToFill.clearActiveBufferRegion();

    float* left = buffer->getWritePointer(0, bufferToFill.startSample);
    float* right = buffer->getNumChannels() > 1
                       ? buffer->getWritePointer(1, bufferToFill.startSample)
                       : nullptr;
    loopPlayer.process(audioRecorder.clip(), audioRecorder.clipLength(), left, right,
                       static_cast<std::size_t>(bufferToFill.numSamples));
}

void MainComponent::playButtonClicked() {
    loopPlayer.play();
}

void MainComponent::stopButtonClicked() {
    loopPlayer.stop();
}
