#pragma once

#include "MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>

// Pass sample rate to recorder and player and prepare buffers.
void MainComponent::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) {
    audioRecorder.prepare(sampleRate);
    loopPlayer.prepare(sampleRate);

    // Construct loop controller once, and after recorder has allocated buffer
    if (!loopController && audioRecorder.capacity() > 0){
        loopController.emplace(1, static_cast<std::int64_t>(audioRecorder.capacity()));   // emplace constructs loop controller inside optional
    }
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
    // Apply UI commands pending in queue at block's start time
    if (loopController) {
        als::AudioCommand command;

        // Prevent indefinite delay of command handling
        for (int i = 0; i < 64; ++i) {
            if (!commandQueue.pop(command)) {
                break;
            }

                const auto previousTrack = loopController->recordingTrack();
                auto next = *loopController;
                const auto result = next.handle(command.command, sampleClock.now(), command.track);
                const auto nextTrack = next.recordingTrack();

                if (!previousTrack && nextTrack && !audioRecorder.beginControlledTake(*nextTrack)) {
                    controllerSnapshot.publishResult(als::Result::Ignored);
                    continue;
                }

                *loopController = next;

                if (previousTrack && !nextTrack) {
                    if (result == als::Result::TakeTooShort) {
                        audioRecorder.clearTrack(*previousTrack);
                    }
                    audioRecorder.finishControlledTake();
                }

                controllerSnapshot.publishResult(result);
        }
    }

    // Calculate samples current take can capture
    int inputSampleCount = bufferToFill.numSamples;

    if (loopController) {
        if (const auto end = loopController->takeEnd()) {
            const auto remaining = std::max<std::int64_t>(0, *end - sampleClock.now());
            inputSampleCount =
                static_cast<int>(std::min<std::int64_t>(inputSampleCount, remaining));
        }
    }
    // Capture microphone input if recording active and copy into audioRecorder buffer using
    // processInput
    const auto changedLength = audioRecorder.processInput(
        buffer->getReadPointer(0, bufferToFill.startSample),
        inputSampleCount);

    if (changedLength.has_value()) {
        loopPlayer.stop();

        if (loopController) {
            if (*changedLength > 0) {
                const bool loaded =
                    loopController->loadSingleTrack(sampleClock.now(), *changedLength);
                jassert(loaded);
                static_cast<void>(loaded);
            } else {
                static_cast<void>(
                    loopController->handle(als::Command::DeleteTrack, sampleClock.now(), 0));
            }
        }
    }

    // The input shares this buffer, so clear it before writing output. Prevents microphone input
    // going direct through to output speakers.
    bufferToFill.clearActiveBufferRegion();

    float* left = buffer->getWritePointer(0, bufferToFill.startSample);
    float* right = buffer->getNumChannels() > 1
                       ? buffer->getWritePointer(1, bufferToFill.startSample)
                       : nullptr;
    loopPlayer.process(audioRecorder.clip(), audioRecorder.clipLength(), left, right,
                       static_cast<std::size_t>(bufferToFill.numSamples));

    // Held at the loudest block until the timer reads it, so a short peak between ticks still
    // shows. A reset between this load and store is overwritten, which only holds it one tick.
    const float level = buffer->getMagnitude(bufferToFill.startSample, bufferToFill.numSamples);
    playbackLevel.store(std::max(playbackLevel.load(), level));

    // Advance the shared clock according to numSmaples processed, this block's end time
    sampleClock.advance(bufferToFill.numSamples);

    // Publish state for the UI to access by updating snapshot
    if (loopController) {
        const auto previousTrack = loopController->recordingTrack();
        loopController->advance(sampleClock.now());

        if (previousTrack && !loopController->recordingTrack()) {
            audioRecorder.finishControlledTake();
        }

        controllerSnapshot.publish(*loopController, sampleClock.now());
    }
}

void MainComponent::playButtonClicked() {
    loopPlayer.play();
}

void MainComponent::stopButtonClicked() {
    loopPlayer.stop();
}
