#include "AudioRecorder.h"

#include <cmath>
#include <limits>
#include <algorithm>

void AudioRecorder::prepare(double sampleRate)
{
    recordingSampleRate = 0.0;
    recordedSampleCount = 0;
    state.store(RecordingState::Idle);

    const double capacity = sampleRate * maximumRecordingSeconds;

    // Check sample rate valid and will fit an int
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 ||
        capacity > std::numeric_limits<int>::max())
    {
        recordingBuffer.setSize(0, 0);
        return;
    }

    recordingBuffer.setSize(1, static_cast<int>(std::ceil(capacity)));   // Allocate space in audio buffer, round up.
    recordingBuffer.clear();   // fill buffer storage with zeros representing silence

    recordingSampleRate = sampleRate;   // store sample rate
}

bool AudioRecorder::requestStart()
{
    auto expected = state.load();   // current state into [expected]

    if (expected != RecordingState::Idle &&
        expected != RecordingState::Ready)
    {
        return false;   // if recorder not idle or is not Ready. Can't start again if already recording.
    }

    // Check state still equal with expected. State may have changed from another thread.
    return state.compare_exchange_strong(
        expected, RecordingState::StartRequested);
}

bool AudioRecorder::requestStop()
{
    auto expected = state.load();   // Read current state

    // Keep trying if start pending or if recording occurring.
    while (expected == RecordingState::StartRequested ||
           expected == RecordingState::Recording)
    {
        if (state.compare_exchange_weak(
                expected, RecordingState::StopRequested))
        {
            return true;   // return True if successfully change state to StopRequested
        }
    }

    return false;
}

void AudioRecorder::processInput(
    const float* inputSamples, int sampleCount)   // point to microphone samples, how many samples available
{
    auto currentState = state.load();   // Get current state

    if (currentState == RecordingState::StartRequested)   // Check if recording requested from UI
    {
        auto expected = RecordingState::StartRequested;

        if (state.compare_exchange_strong(
                expected, RecordingState::Recording))   // Change state to Recording if still StartRequested
        {
            recordedSampleCount = 0;   // If state changed to recording, buffer write position reset to 0 to overwrite previous
            captureActive = true;
        }

        currentState = state.load();   // Reread current state for cases where Stop request may have occurred
    }

    if (currentState == RecordingState::StopRequested)   // Handle if recording Stop requested
    {
        if (!captureActive)   // Check if audio capture actually started. Case where user presses Record then Stop rapidly.
        {
            recordedSampleCount = 0;
        }

        captureActive = false;
        state.store(RecordingState::Ready);   // State to Ready
        return;
    }

    if (currentState != RecordingState::Recording)   // Ignore Stop if not recording
    {
        return;
    }

    const int remaining =
        recordingBuffer.getNumSamples() - recordedSampleCount;   // Calculate remaining buffer capacity

    if (remaining <= 0)   // If buffer full, finish recording take and exit to avoid writing beyond capacity
    {
        captureActive = false;
        state.store(RecordingState::Ready);
        return;
    }

    if (inputSamples == nullptr || sampleCount <= 0)  // If samples pointer not pointing to audio data, or if no samples to copy, exit
    {
        return;
    }

    const int samplesToCopy = std::min(sampleCount, remaining);   // Must limit sample amount to copy to remaining space

    recordingBuffer.copyFrom(
        0, recordedSampleCount, inputSamples, samplesToCopy);   // Copy inputSamples from microphone to channel 0

    recordedSampleCount += samplesToCopy;   // Advance write position based on number of samples copied

    // Finish if recording buffer full
    if (recordedSampleCount == recordingBuffer.getNumSamples())
    {
        captureActive = false;
        state.store(RecordingState::Ready);   // Update state to Ready
    }
}

// Return current snapshot of recorder's state for UI to read
AudioRecorder::RecordingState AudioRecorder::getState() const noexcept
{
    return state.load();
}