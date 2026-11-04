#include "AudioRecorder.h"

#include <algorithm>
#include <cmath>
#include <limits>

void AudioRecorder::prepare(double sampleRate) {
    recordingSampleRate = 0.0;
    recordedSampleCount = 0;
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
        state.store(RecordingState::Ready); // State to Ready
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
        state.store(RecordingState::Ready);
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
        state.store(RecordingState::Ready); // Update state to Ready
    }
}

// Return current snapshot of recorder's state for UI to read
AudioRecorder::RecordingState AudioRecorder::getState() const noexcept {
    return state.load();
}

juce::Result AudioRecorder::saveToWav(const juce::File& destination) {
    auto expected = RecordingState::Ready; // Only save when recording ready

    // Only save if finished recording. If state Ready, change to Recording.
    if (!state.compare_exchange_strong(expected, RecordingState::Saving)) {
        return juce::Result::fail("Finish recording before saving.");
    }

    const auto result = [&]() -> juce::Result {
        // Check actual clip to save
        if (recordedSampleCount <= 0 || recordingSampleRate <= 0.0) {
            return juce::Result::fail("There is no recorded audio to save.");
        }

        // Create temp file before replacing at destination one writing succeeds
        juce::TemporaryFile temporaryFile(destination);
        auto stream = temporaryFile.getFile()
                          .createOutputStream(); // output stream to write bytes to WAV file

        // If file opening fails
        if (stream == nullptr) {
            return juce::Result::fail("Could not open the file for writing.");
        }

        juce::WavAudioFormat wavFormat; // WavAudioFormat organizes audio into WAV file

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(stream.get(), recordingSampleRate,
                                      1,  // 1 channel
                                      24, // Samples save with 24-bit
                                      {}, // No added metadata
                                      0));

        // If writer creation fails
        if (writer == nullptr) {
            return juce::Result::fail("Could not create the WAV writer.");
        }

        // The wav writer owns stream. unique_ptr no longer does
        stream.release();

        // Write samples from recording Buffer starting at 0 up to actual recordedSampleCount.
        // Returns result T/F
        const bool written =
            writer->writeFromAudioSampleBuffer(recordingBuffer, 0, recordedSampleCount);

        // Close the writer and finish the WAV header.
        writer.reset();

        if (!written) {
            return juce::Result::fail("Could not write the recorded audio.");
        }

        // Move temp file to destination, replace if needed.
        if (!temporaryFile.overwriteTargetFileWithTemporary()) {
            return juce::Result::fail("Could not save to the selected location.");
        }

        return juce::Result::ok();
    }();

    state.store(RecordingState::Ready); // Update Recording state
    return result;
}