#pragma once

#include "MainComponent.h"
#include "core/LoopPlayback.h"
#include "core/PanLaw.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>

namespace {

constexpr double kGainRampSeconds = 0.02; // [s], long enough that a control change won't click
constexpr int edgeGainMilliseconds = 5;   // [ms] at each end of the loop, so the wrap doesn't click

static_assert(als::kLiveInputChunk == TrackLane::kSamplesPerThumbnailPoint,
              "a live input chunk has to fit in one thumbnail point");

} // namespace

// Pass sample rate to recorder and player and prepare buffers.
void MainComponent::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) {
    // A new rate clears and resizes the recorder's tracks, so the controller has to start over.
    const bool newRate = sampleRate != audioSampleRate;
    audioSampleRate = sampleRate;
    audioRecorder.prepare(sampleRate);

    // Whenver JUCE prepares audio device, refresh latency estimate
    // If no device, leave at 0.
    recordingLatencySamples = 0;

    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        const int inputLatency = std::max(0, device->getInputLatencyInSamples());
        const int outputLatency = std::max(0, device->getOutputLatencyInSamples());

        // Only device reported latency here. User adjustment will be added when each new take
        // starts.
        recordingLatencySamples = static_cast<std::int64_t>(inputLatency) + outputLatency;

        // Show devices reported latency in Debug run
        DBG("Input latency: " + juce::String(inputLatency) + " samples; output latency: " +
            juce::String(outputLatency) + " samples; total compensation: " +
            juce::String(static_cast<double>(recordingLatencySamples) * 1000.0 / sampleRate) +
            " ms");
    }

    // Starts each track at its current settings, so playback doesn't ramp in from silence.
    if (sampleRate > 0.0) {
        updateGainTargets();
        for (TrackGains& gains : trackGains) {
            // A reset also jumps each gain straight to the target just set.
            gains.volume.reset(sampleRate, kGainRampSeconds);
            gains.left.reset(sampleRate, kGainRampSeconds);
            gains.right.reset(sampleRate, kGainRampSeconds);
        }

        for (std::size_t track = 0; track < als::kTrackCount; ++track) {
            auto& playbackFade = trackPlaybackFades[track];
            playbackFade.activeReversed = trackReversed[track].load();
            playbackFade.activeSlipSamples =
                als::millisecondsToSamples(trackSlips[track].load(), sampleRate);
            playbackFade.phase = TrackPlaybackFade::Phase::Steady;
            playbackFade.gain.reset(sampleRate, kGainRampSeconds);
            playbackFade.gain.setCurrentAndTargetValue(1.0f);
        }
    }

    // Construct loop controller after recorder has allocated buffer, and again for a new rate. The
    // audio callback is stopped while this runs, so replacing it is safe.
    if ((!loopController || newRate) && audioRecorder.capacity() > 0) {
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
                trackSlips[command.track].store(
                    0); // Replacement recording should start with 0 slip

                // Replacement recording should start with reverse off.
                trackReversed[command.track].store(false);
                auto& playbackFade = trackPlaybackFades[command.track];
                playbackFade.activeReversed = false;
                playbackFade.activeSlipSamples = 0;
                playbackFade.phase = TrackPlaybackFade::Phase::Steady;
                playbackFade.gain.setCurrentAndTargetValue(1.0f);
            }

            *loopController = next;

            // New recording take, save loop position once
            // Save the new take's position once. Existing takes aren't moved.
            if (!previousTrack && nextTrack) {
                const auto& loop = loopController->loop();

                if (loop) {
                    const auto position = als::loopPosition(*loop, sampleClock.now());

                    // Compensation for delay between speaker playback and receiving from microphone
                    // audio Postition take earlier by device's latency, wrap appropriately Read the
                    // adjustment once for this take. Changing the control later won't move an
                    // existing recording.
                    const auto extraSamples =
                        als::millisecondsToSamples(recordingAdjustmentMs.load(), audioSampleRate);

                    const auto totalLatency = recordingLatencySamples + extraSamples;

                    // Position this take earlier by the combined latency compensation. Negative
                    // compensation moves later. Wrap appropriately.
                    trackStartPositions[*nextTrack] = static_cast<std::int64_t>(
                        als::trackWriteIndex(position, loop->length, totalLatency));
                } else {
                    // The first take defines the loop; no playback reference exists yet.
                    trackStartPositions[*nextTrack] = 0;
                }
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

    // Before the output clear below, which overwrites the input.
    if (loopController) {
        publishLiveInput(buffer->getReadPointer(0, bufferToFill.startSample), inputSampleCount);
    }

    // A load replaced every track this block, so each take's start and the loop come from it.
    if (loaded.has_value()) {
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

    const std::size_t loopLength = static_cast<std::size_t>(timing.length);
    const std::size_t fadeLength =
        static_cast<std::size_t>(audioSampleRate * edgeGainMilliseconds / 1000.0);

    // REV setting read once and stays consistent through this audio block
    std::array<bool, als::kTrackCount> reversed{};

    // Each slip read & translated to samples once per block
    std::array<std::int64_t, als::kTrackCount> slipSamples{};

    for (std::size_t track = 0; track < als::kTrackCount; ++track) {
        reversed[track] = trackReversed[track].load();
        slipSamples[track] = als::millisecondsToSamples(trackSlips[track].load(), audioSampleRate);
    }

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

            auto& playbackFade = trackPlaybackFades[track];

            // A silent track can adopt reversal or slip changes without fading.
            if (!loopController->audible(track, now)) {
                playbackFade.activeReversed = reversed[track];
                playbackFade.activeSlipSamples = slipSamples[track];
                playbackFade.phase = TrackPlaybackFade::Phase::Steady;
                playbackFade.gain.setCurrentAndTargetValue(1.0f);
                continue;
            }

            // Fade before reversing or slipping to avoid audible clicks.
            if (playbackFade.phase == TrackPlaybackFade::Phase::Steady &&
                (reversed[track] != playbackFade.activeReversed ||
                 slipSamples[track] != playbackFade.activeSlipSamples)) {
                playbackFade.phase = TrackPlaybackFade::Phase::FadeOut;
                playbackFade.gain.setTargetValue(0.0f);
            }

            // Converts loop position to index in track's buffer.
            const auto takeStart = trackStartPositions[track].load();

            // Subtrack start position to align with shared loop. trackReadIndex wraps and if REV is
            // on with read backwards. Settings switch after the fade-out reaches silence.
            const auto index =
                als::trackReadIndex(position - takeStart, timing.length,
                                    playbackFade.activeSlipSamples, playbackFade.activeReversed);

            const float edge = als::edgeGain(index, loopLength, fadeLength);
            const float playbackGain = playbackFade.gain.getNextValue();

            // Add each track's contribution to the mix & track its peak, after volume and
            // before pan so the meter follows the volume slider
            const float sample = audioRecorder.sampleAt(track, static_cast<int>(index)) * volume *
                                 edge * playbackGain;

            // Reverse or slip once the fade-out reaches silence.
            if (playbackFade.phase == TrackPlaybackFade::Phase::FadeOut &&
                !playbackFade.gain.isSmoothing()) {
                playbackFade.activeReversed = reversed[track];
                playbackFade.activeSlipSamples = slipSamples[track];
                playbackFade.phase = TrackPlaybackFade::Phase::FadeIn;
                playbackFade.gain.setTargetValue(1.0f);
            } else if (playbackFade.phase == TrackPlaybackFade::Phase::FadeIn &&
                       !playbackFade.gain.isSmoothing()) {
                playbackFade.phase = TrackPlaybackFade::Phase::Steady;
            }

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

void MainComponent::publishLiveInput(const float* input, int sampleCount) {
    jassert(loopController.has_value());
    const auto track = loopController->recordingTrack();
    const auto take = loopController->takeStart();
    if (!track || !take || input == nullptr) {
        return;
    }

    const auto now = sampleClock.now();
    const auto& loop = loopController->loop();
    als::LiveInput chunk;
    chunk.track = *track;
    chunk.take = *take;
    chunk.loopLength = loop ? loop->length : 0;

    for (int done = 0; done < sampleCount; done += chunk.count) {
        // Each sample goes where it will play, the same place the finished waveform draws it.
        const auto time = now + done;
        chunk.position = loop ? als::loopPosition(*loop, time) : time - *take;

        // A chunk ends at the next thumbnail point or the loop's end, whichever comes first.
        chunk.count = static_cast<int>(als::samplesToChunkEnd(
            chunk.position, chunk.loopLength, als::kLiveInputChunk, sampleCount - done));
        std::copy_n(input + done, chunk.count, chunk.samples.begin());

        // A full queue means the UI is behind. The take's finished waveform fills the gap.
        if (!liveInputQueue.push(chunk)) {
            return;
        }
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
