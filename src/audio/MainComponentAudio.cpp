#pragma once

#include "MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>

// Pass sample rate to recorder and player and prepare buffers.
void MainComponent::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) {
    audioRecorder.prepare(sampleRate);
    loopPlayer.prepare(sampleRate);

    // Construct loop controller once, and after recorder has allocated buffer
    if (!loopController && audioRecorder.capacity() > 0) {
        loopController.emplace(
            1, static_cast<std::int64_t>(
                   audioRecorder.capacity())); // emplace constructs loop controller inside optional
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

            if (command.command == als::Command::DeleteTrack && result == als::Result::Accepted) {
                // track marked as empty if can clear audio
                // if recorder busy, skip delete and just leave as is
                if (!audioRecorder.tryDeleteTrack(command.track)) {
                    controllerSnapshot.publishResult(als::Result::Ignored);
                    continue;
                }

                trackStartPositions[command.track] = 0;
            }

            *loopController = next;

            // New recording take, save loop position once
            if (!previousTrack && nextTrack) {
                const auto& loop = loopController->loop();
                trackStartPositions[*nextTrack] =
                    loop ? als::loopPosition(*loop, sampleClock.now())
                         : 0; // If no loop, save 0 as start position. If loop, save track start
                              // position within that loop.
            }

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
    const auto loaded = audioRecorder.processInput(
        buffer->getReadPointer(0, bufferToFill.startSample), inputSampleCount);

    // A load replaced every track this block, so each take's start and the loop come from it.
    if (loaded.has_value()) {
        loopPlayer.stop();

        for (std::size_t track = 0; track < als::kTrackCount; ++track) {
            trackStartPositions[track] = loaded->starts[track];
        }
        if (loopController) {
            const bool restarted =
                loopController->loadTracks(sampleClock.now(), loaded->loopLength, loaded->hasAudio);
            jassert(restarted);
            static_cast<void>(restarted);
        }
    }

    // The input shares this buffer, so clear it before writing output. Prevents microphone input
    // going direct through to output speakers.
    bufferToFill.clearActiveBufferRegion();

    float* left = buffer->getWritePointer(0, bufferToFill.startSample);
    float* right = buffer->getNumChannels() > 1
                       ? buffer->getWritePointer(1, bufferToFill.startSample)
                       : nullptr;

    if (loopController) {
        const auto blockStart = sampleClock.now(); // Sample # at start of this current audio block
        auto timing = loopController->loop();      // Loop's starting time and length

        // Handles case if 1st take fills buffer partway through block.
        if (!timing) {
            const auto start = loopController->takeStart();
            const auto end = loopController->takeEnd();
            if (start && end && *end < blockStart + bufferToFill.numSamples) {
                timing = als::LoopTiming{*start, *end - *start};
            }
        }

        if (timing) {
            std::array<float, als::kTrackCount>
                blockTrackPeaks{}; // holds loudest sample of each track per block
            for (int i = 0; i < bufferToFill.numSamples; ++i) {
                const auto now = blockStart + i;
                const auto position = als::loopPosition(
                    *timing,
                    now); // Converts shared clock time to position that will repeat each loop
                float mixed =
                    0.0f; // Start with silence but will add each track's audio part at instant

                for (std::size_t track = 0; track < als::kTrackCount; ++track) {
                    // Skip muted track
                    if (!loopController->audible(track, now)) {
                        continue;
                    }

                    // Converts loop position to index in track's buffer.
                    const auto takeStart = trackStartPositions[track].load();
                    auto index = position - takeStart;

                    // Wrap negative index
                    if (index < 0) {
                        index += timing->length;
                    }

                    // Add each track's contribution to mixed & track it's peak sample
                    const float sample = audioRecorder.sampleAt(track, static_cast<int>(index));
                    blockTrackPeaks[track] = std::max(blockTrackPeaks[track], std::abs(sample));
                    mixed += sample;
                }

                const float output = std::clamp(mixed, -1.0f, 1.0f);
                left[i] = output;
                if (right != nullptr) {
                    right[i] = output;
                }
            }

            // Held at the loudest block until the timer reads it, so a short peak between ticks
            // still shows. A reset between this load and store is overwritten, which only holds it
            // one tick.
            for (size_t i = 0; i < als::kTrackCount; i++) {
                trackPeaks[i].store(std::max(trackPeaks[i].load(), blockTrackPeaks[i]));
            }
        }
    }

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

    // Release pairs with the acquire in copyLoop() and updateWaveform(), so a UI that sees this
    // count also sees the take starts and snapshot this block wrote for it.
    publishedClips.store(audioRecorder.finishedClips(), std::memory_order_release);
}

void MainComponent::playButtonClicked() {
    if (!commandQueue.push({als::Command::Play, 0})) {
        DBG("Could not queue Play: audio command queue is full");
    }
}

void MainComponent::stopButtonClicked() {
    if (!commandQueue.push({als::Command::Stop, 0})) {
        DBG("Could not queue Stop: audio command queue is full");
    }
}
