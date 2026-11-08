#include "AudioRecorder.h"

#include <algorithm>
#include <cmath>
#include <limits>

void AudioRecorder::prepare(double sampleRate) {
    // A device restart at the same rate keeps the clip, so only reset when the rate changes.
    if (sampleRate == recordingSampleRate && tracks.capacity() > 0) {
        return;
    }

    recordingSampleRate = 0.0;
    captureActive = false;
    clipPending.store(false); // a pending clip no longer matches the new rate
    state.store(RecordingState::Idle);

    // Clear existing storage with sample rate changes
    tracks.prepare(0);

    const double capacity = sampleRate * maximumRecordingSeconds;

    // Check sample rate valid and will fit an int
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 ||
        capacity > std::numeric_limits<int>::max()) {
        return;
    }

    tracks.prepare(static_cast<int>(std::ceil(capacity)));
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

std::optional<int> AudioRecorder::processInput(
    const float* inputSamples,
    int sampleCount) // point to microphone samples, how many samples available
{
    auto currentState = state.load(); // Get current state

    // Loads and deletes are applied here, so the buffer is only written on the audio thread.
    if (currentState == RecordingState::Loading) {
        return applyPendingClip();
    }

    if (currentState == RecordingState::StartRequested) // Check if recording requested from UI
    {
        auto expected = RecordingState::StartRequested;

        if (state.compare_exchange_strong(
                expected,
                RecordingState::Recording)) // Change state to Recording if still StartRequested
        {
            activeTrack = 0;
            tracks.clearTrack(0);   // If state changed to recording, buffer write position reset
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
            tracks.clearTrack(0);
        }

        captureActive = false;
        finishClip(); // State to Ready
        return std::nullopt;
    }

    if (currentState != RecordingState::Recording) // Ignore Stop if not recording
    {
        return std::nullopt;
    }

        if (tracks.recordedLength(activeTrack) >= tracks.capacity()) {
        captureActive = false;
        finishClip();
        return std::nullopt;
    }

    if (inputSamples == nullptr || sampleCount <= 0) {
        return std::nullopt;
    }

    const int copied = tracks.appendSamples(activeTrack, inputSamples, sampleCount);

    if (copied > 0 && tracks.recordedLength(activeTrack) == tracks.capacity()) {
        captureActive = false;
        finishClip();
    }
    return std::nullopt;
}

void AudioRecorder::clearTrack(std::size_t track) noexcept {
    // Prepares next empty track for new recording
    tracks.clearTrack(track);
}

bool AudioRecorder::tryDeleteTrack(std::size_t track) noexcept {
    if (track >= als::kTrackCount) {
        return false;
    }

    auto expected = RecordingState::Ready;

    // Only delete if recorder Ready, then switch to Loading.  UI can't copy samples while track being cleared/deleted
    if (!state.compare_exchange_strong(expected, RecordingState::Loading)) {
        return false;
    }

    tracks.clearTrack(track);

    finishClip();   // UI clears waveform, returns recorder to Ready
    return true;
}

int AudioRecorder::appendToTrack(std::size_t track, const float* inputSamples, int sampleCount) noexcept {
    // Write samples of microphone to track selected by controller
    return tracks.appendSamples(track, inputSamples, sampleCount);
}

float AudioRecorder::sampleAt(std::size_t track, int position) const noexcept {
    // Reads track's audio
    return tracks.sampleAt(track, position);
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
    return tracks.data(0);
}

std::size_t AudioRecorder::clipLength() const noexcept {
    // Saving only reads the clip, so it can still play while it's copied.
    const auto current = state.load();
    const bool finished = current == RecordingState::Ready || current == RecordingState::Saving;
    return finished ? static_cast<std::size_t>(tracks.recordedLength(0)) : 0;
}

std::optional<int> AudioRecorder::applyPendingClip() noexcept {
    if (!clipPending.load(std::memory_order_acquire)) {
        return std::nullopt;
    }

    tracks.clearTrack(0);

    if (!pendingClip.empty()) {
        // Clear previous tracks if loading previously saved loop. TO update as existing functionality for saving only 1 track not whole loop.
        for (std::size_t track = 1; track < als::kTrackCount; ++track) {
            tracks.clearTrack(track);
        }
        const int copied = tracks.appendSamples(
            0, pendingClip.data(), static_cast<int>(pendingClip.size()));

        jassert(copied == static_cast<int>(pendingClip.size()));
        static_cast<void>(copied);
    }

    clipPending.store(false, std::memory_order_relaxed);
    finishClip();
    return tracks.recordedLength(0);
}

juce::Result AudioRecorder::loadClip(const std::vector<float>& samples, double sampleRate) {
    // The clip plays at the device's rate, so a loop saved at another rate would play off pitch.
    if (juce::roundToInt(sampleRate) != juce::roundToInt(recordingSampleRate)) {
        return juce::Result::fail("This loop was saved at " +
                                  juce::String(juce::roundToInt(sampleRate)) +
                                  " Hz, but the audio device runs at " +
                                  juce::String(juce::roundToInt(recordingSampleRate)) + " Hz.");
    }
    if (samples.empty() ||
        samples.size() > static_cast<std::size_t>(tracks.capacity())) {
        return juce::Result::fail("The loop is empty or longer than the recorder can hold.");
    }

    return handOverClip(samples, "Finish recording before loading a loop.");
}

juce::Result AudioRecorder::clearClip() {
    // An empty clip goes through the same handover, so the buffer is still only changed on the
    // audio thread.
    return handOverClip({}, "Finish recording before deleting the track.");
}

juce::Result AudioRecorder::handOverClip(const std::vector<float>& samples,
                                         const juce::String& busyMessage) {
    // Loading keeps a new take from starting until the audio thread has copied the clip in.
    auto expected = state.load();
    if ((expected != RecordingState::Idle && expected != RecordingState::Ready) ||
        !state.compare_exchange_strong(expected, RecordingState::Loading)) {
        return juce::Result::fail(busyMessage);
    }
    pendingClip.assign(samples.begin(), samples.end());
    // Pairs with the acquire in applyPendingClip.
    clipPending.store(true, std::memory_order_release);
    return juce::Result::ok();
}

juce::Result AudioRecorder::copyRecording(std::vector<float>& samples, double& sampleRate) {
    auto expected = RecordingState::Ready; // Only copy when recording ready

    // Saving keeps the audio thread from starting a new take while the clip is copied.
    if (!state.compare_exchange_strong(expected, RecordingState::Saving)) {
        return juce::Result::fail("Finish recording before saving.");
    }

    const int recordedSampleCount = tracks.recordedLength(0);

    // Check actual clip to copy
    if (recordedSampleCount <= 0 || recordingSampleRate <= 0.0) {
        state.store(RecordingState::Ready);
        return juce::Result::fail("There is no recorded audio to save.");
    }

    const float* recorded = tracks.data(0);
    samples.assign(recorded, recorded + recordedSampleCount);
    sampleRate = recordingSampleRate;

    state.store(RecordingState::Ready); // Update Recording state
    return juce::Result::ok();
}

bool AudioRecorder::copyWaveformTracks(
    std::array<std::vector<float>, als::kTrackCount>& samples, double& sampleRate) {
    auto expected = RecordingState::Ready;

    // Protection for if copying, load or new recording can't claim buffers
    if (!state.compare_exchange_strong(expected, RecordingState::Saving)) {
        return false;
    }

    sampleRate = recordingSampleRate;

    for (std::size_t track = 0; track < als::kTrackCount; ++track) {
        const int length = tracks.recordedLength(track);

        if (length == 0) {
            samples[track].clear();
            continue;
        }

        // Only copy recorded samples, don't need whole buffer
        const float* first = tracks.data(track);
        samples[track].assign(first, first + length);
    }

    // The UI has copies to create waveforms. Buffers not needed.
    state.store(RecordingState::Ready);
    return true;
}

bool AudioRecorder::beginControlledTake(std::size_t track) noexcept {
    if (track >= als::kTrackCount || tracks.capacity() <= 0) {
        return false;
    }

    auto expected = state.load();
    if (expected != RecordingState::Idle && expected != RecordingState::Ready) {
        return false;
    }

    if (!state.compare_exchange_strong(expected, RecordingState::Recording)) {
        return false;
    }

    activeTrack = track;
    tracks.clearTrack(activeTrack);
    captureActive = true;
    return true;
}

void AudioRecorder::finishControlledTake() noexcept {
    if (state.load() == RecordingState::Recording) {
        captureActive = false;
        finishClip();
    }
}
