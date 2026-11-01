#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class AudioRecorder
{
public:
    enum class RecordingState
        {
            Idle,
            StartRequested,
            Recording,
            StopRequested,
            Ready
        };
    AudioRecorder() = default;   // C++ to provide normal constructor
    AudioRecorder(const AudioRecorder&) = delete;   // Prevent copying of recorder
    AudioRecorder& operator=(const AudioRecorder&) = delete;

    void prepare(double sampleRate);   // allocate buffer amount
    bool requestStart();   // returned boolean will indicate if request accepted
    bool requestStop();
    void processInput(const float* inputSamples, int sampleCount);

    RecordingState getState() const noexcept;

private:
    // std::atomic allows state to be read across threads and changed
    std::atomic<RecordingState> state { RecordingState::Idle };

    juce::AudioBuffer<float> recordingBuffer;   // hold recorded audio in memory

    double recordingSampleRate = 0.0;   // store rate, needed to interpret and save recorded sample
    int recordedSampleCount = 0;   // will record how much of buffer contains audio sample
    static constexpr int maximumRecordingSeconds = 60;   // capacity limit, can change

    bool captureActive = false; // Used by audio thread
};