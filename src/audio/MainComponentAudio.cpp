#pragma once

#include "MainComponent.h"
#include "core/PanLaw.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>

namespace {

constexpr double kGainRampSeconds = 0.02; // [s], long enough that a control change won't click

} // namespace

// Pass sample rate to recorder and player and prepare buffers.
void MainComponent::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) {
    audioRecorder.prepare(sampleRate);
    loopPlayer.prepare(sampleRate);

    // Starts each track at its current settings, so playback doesn't ramp in from silence.
    if (sampleRate > 0.0) {
        updateGainTargets();
        for (TrackGains& gains : trackGains) {
            // A reset also jumps each gain straight to the target just set.
            gains.volume.reset(sampleRate, kGainRampSeconds);
            gains.left.reset(sampleRate, kGainRampSeconds);
            gains.right.reset(sampleRate, kGainRampSeconds);
        }
    }

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
            mixTracks(*timing, blockStart, bufferToFill.numSamples, left, right);
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

void MainComponent::updateGainTargets() {
    for (std::size_t track = 0; track < als::kTrackCount; ++track) {
        const als::PanGains pan = als::panGains(trackPans[track]);
        trackGains[track].volume.setTargetValue(trackMuted[track] ? 0.0f : trackVolumes[track]);
        trackGains[track].left.setTargetValue(pan.left);
        trackGains[track].right.setTargetValue(pan.right);
    }
}

void MainComponent::mixTracks(const als::LoopTiming& timing, std::int64_t blockStart,
                              int sampleCount, float* left, float* right) {
    jassert(loopController.has_value());
    updateGainTargets();

    std::array<float, als::kTrackCount>
        blockTrackPeaks{}; // holds loudest sample of each track per block
    for (int i = 0; i < sampleCount; ++i) {
        const auto now = blockStart + i;
        // Converts shared clock time to position that will repeat each loop
        const auto position = als::loopPosition(timing, now);
        float mixedLeft = 0.0f; // Start with silence, then add each track's share
        float mixedRight = 0.0f;
        float mixedMono = 0.0f; // for a device with one output, which has no pan

        for (std::size_t track = 0; track < als::kTrackCount; ++track) {
            // Eased even while muted, so unmuting doesn't jump to a setting changed since.
            const float volume = trackGains[track].volume.getNextValue();
            const float leftGain = trackGains[track].left.getNextValue();
            const float rightGain = trackGains[track].right.getNextValue();

            // Skip a track that isn't playing. Mute is handled by the volume gain above.
            if (!loopController->audible(track, now)) {
                continue;
            }

            // Converts loop position to index in track's buffer.
            const auto takeStart = trackStartPositions[track].load();
            auto index = position - takeStart;

            // Wrap negative index
            if (index < 0) {
                index += timing.length;
            }

            // Add each track's contribution to the mix & track its peak, after volume and
            // before pan so the meter follows the volume slider
            const float sample = audioRecorder.sampleAt(track, static_cast<int>(index)) * volume;
            blockTrackPeaks[track] = std::max(blockTrackPeaks[track], std::abs(sample));
            mixedLeft += sample * leftGain;
            mixedRight += sample * rightGain;
            mixedMono += sample;
        }

        if (right != nullptr) {
            left[i] = std::clamp(mixedLeft, -1.0f, 1.0f);
            right[i] = std::clamp(mixedRight, -1.0f, 1.0f);
        } else {
            left[i] = std::clamp(mixedMono, -1.0f, 1.0f);
        }
    }

    // Held at the loudest block until the timer reads it, so a short peak between ticks
    // still shows. A reset between this load and store is overwritten, which only holds it
    // one tick.
    for (size_t i = 0; i < als::kTrackCount; i++) {
        trackPeaks[i].store(std::max(trackPeaks[i].load(), blockTrackPeaks[i]));
    }
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
