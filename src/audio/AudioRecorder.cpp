#include "AudioRecorder.h"

#include <algorithm>
#include <cmath>
#include <limits>

void AudioRecorder::prepare(double sampleRate) {
    // A device restart at the same rate keeps the clip, so only reset when the rate changes.
    if (sampleRate == recordingSampleRate && recordingBuffer.getNumSamples() > 0) {
        return;
    }

    recordingSampleRate = 0.0;
    recordedSampleCount = 0;
    clipPending.store(false); // a pending clip no longer matches the new rate
    state.store(RecordingState::Idle);

    const double capacity = sampleRate * maximumRecordingSeconds;

    // Check sample rate valid and will fit an int
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 ||
        capacity > std::numeric_limits<int>::max()) {
        recordingBuffer.setSize(0, 0);
        return;
    }

    recordingBuffer.setSize(
        1, static_cast<int>(std::ceil(capacity))); // Allocate space in audio buffer, round up.
    recordingBuffer.clear(); // fill buffer storage with zeros representing silence

    recordingSampleRate = sampleRate; // store sample rate
}

bool AudioRecorder::requestStart() {
    auto expected = state.load(); // current state into [expected]

    if (expected != RecordingState::Idle && expected != RecordingState::Ready) {
        return false; // if recorder not idle or is not Ready. Can't start again if already
                      // recording.
    }

    // Check state still equal with expected. State may have changed from another thread.
    return state.compare_exchange_strong(expected, RecordingState::StartRequested);
}

bool AudioRecorder::requestStop() {
    auto expected = state.load(); // Read current state

    // Keep trying if start pending or if recording occurring.
    while (expected == RecordingState::StartRequested || expected == RecordingState::Recording) {
        if (state.compare_exchange_weak(expected, RecordingState::StopRequested)) {
            return true; // return True if successfully change state to StopRequested
        }
    }

    return false;
}

void AudioRecorder::processInput(
    const float* inputSamples,
    int sampleCount) // point to microphone samples, how many samples available
{
    auto currentState = state.load(); // Get current state

    // A loaded clip is copied in here, so the buffer is only ever written on the audio thread.
    if (currentState == RecordingState::Loading) {
        applyPendingClip();
        return;
    }

    if (currentState == RecordingState::StartRequested) // Check if recording requested from UI
    {
        auto expected = RecordingState::StartRequested;

        if (state.compare_exchange_strong(
                expected,
                RecordingState::Recording)) // Change state to Recording if still StartRequested
        {
            recordedSampleCount = 0; // If state changed to recording, buffer write position reset
                                     // to 0 to overwrite previous
            captureActive = true;
        }

        currentState =
            state.load(); // Reread current state for cases where Stop request may have occurred
    }

    if (currentState == RecordingState::StopRequested) // Handle if recording Stop requested
    {
        if (!captureActive) // Check if audio capture actually started. Case where user presses
                            // Record then Stop rapidly.
        {
            recordedSampleCount = 0;
        }

        captureActive = false;
        finishClip(); // State to Ready
        return;
    }

    if (currentState != RecordingState::Recording) // Ignore Stop if not recording
    {
        return;
    }

    const int remaining = recordingBuffer.getNumSamples() -
                          recordedSampleCount; // Calculate remaining buffer capacity

    if (remaining <=
        0) // If buffer full, finish recording take and exit to avoid writing beyond capacity
    {
        captureActive = false;
        finishClip();
        return;
    }

    if (inputSamples == nullptr ||
        sampleCount <=
            0) // If samples pointer not pointing to audio data, or if no samples to copy, exit
    {
        return;
    }

    const int samplesToCopy =
        std::min(sampleCount, remaining); // Must limit sample amount to copy to remaining space

    recordingBuffer.copyFrom(0, recordedSampleCount, inputSamples,
                             samplesToCopy); // Copy inputSamples from microphone to channel 0

    recordedSampleCount +=
        samplesToCopy; // Advance write position based on number of samples copied

    // Finish if recording buffer full
    if (recordedSampleCount == recordingBuffer.getNumSamples()) {
        captureActive = false;
        finishClip(); // Update state to Ready
    }
}

void AudioRecorder::finishClip() noexcept {
    // The count goes up first, so a UI that sees Ready never misses the clip it belongs to.
    clipCount.fetch_add(1);
    state.store(RecordingState::Ready);
}

std::uint32_t AudioRecorder::finishedClips() const noexcept {
    return clipCount.load();
}

// Return current snapshot of recorder's state for UI to read
AudioRecorder::RecordingState AudioRecorder::getState() const noexcept {
    return state.load();
}

const float* AudioRecorder::clip() const noexcept {
    return recordingBuffer.getNumSamples() > 0 ? recordingBuffer.getReadPointer(0) : nullptr;
}

std::size_t AudioRecorder::clipLength() const noexcept {
    // Saving only reads the clip, so it can still play while it's copied.
    const auto current = state.load();
    const bool finished = current == RecordingState::Ready || current == RecordingState::Saving;
    return finished ? static_cast<std::size_t>(recordedSampleCount) : 0;
}

void AudioRecorder::applyPendingClip() noexcept {
    if (!clipPending.load(std::memory_order_acquire)) {
        return;
    }
    // An empty clip clears the track, and the buffer may have no channel to write to.
    if (!pendingClip.empty()) {
        std::copy(pendingClip.begin(), pendingClip.end(), recordingBuffer.getWritePointer(0));
    }
    recordedSampleCount = static_cast<int>(pendingClip.size());
    clipPending.store(false, std::memory_order_relaxed);
    finishClip();
}

juce::Result AudioRecorder::loadClip(const std::vector<float>& samples, double sampleRate) {
    // The clip plays at the device's rate, so one saved at another rate would play at the wrong
    // pitch.
    if (juce::roundToInt(sampleRate) != juce::roundToInt(recordingSampleRate)) {
        return juce::Result::fail("This loop was saved at " +
                                  juce::String(juce::roundToInt(sampleRate)) +
                                  " Hz, but the audio device runs at " +
                                  juce::String(juce::roundToInt(recordingSampleRate)) + " Hz.");
    }
    if (samples.empty() ||
        samples.size() > static_cast<std::size_t>(recordingBuffer.getNumSamples())) {
        return juce::Result::fail("The loop is empty or longer than the recorder can hold.");
    }

    // Loading keeps a new take from starting until the audio thread has copied the clip in.
    auto expected = state.load();
    if ((expected != RecordingState::Idle && expected != RecordingState::Ready) ||
        !state.compare_exchange_strong(expected, RecordingState::Loading)) {
        return juce::Result::fail("Finish recording before loading a loop.");
    }
    pendingClip.assign(samples.begin(), samples.end());
    clipPending.store(true, std::memory_order_release); // pairs with the acquire in processInput
    return juce::Result::ok();
}

juce::Result AudioRecorder::copyRecording(std::vector<float>& samples, double& sampleRate) {
    auto expected = RecordingState::Ready; // Only copy when recording ready

    // Saving keeps the audio thread from starting a new take while the clip is copied.
    if (!state.compare_exchange_strong(expected, RecordingState::Saving)) {
        return juce::Result::fail("Finish recording before saving.");
    }

    // Check actual clip to copy
    if (recordedSampleCount <= 0 || recordingSampleRate <= 0.0) {
        state.store(RecordingState::Ready);
        return juce::Result::fail("There is no recorded audio to save.");
    }

    const float* recorded = recordingBuffer.getReadPointer(0);
    samples.assign(recorded, recorded + recordedSampleCount);
    sampleRate = recordingSampleRate;

    state.store(RecordingState::Ready); // Update Recording state
    return juce::Result::ok();
}
