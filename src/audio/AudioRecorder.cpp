#include "AudioRecorder.h"

#include <cmath>
#include <limits>

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