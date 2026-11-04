#pragma once

#include <atomic>
#include <cstddef>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

class AudioRecorder {
  public:
    enum class RecordingState { Idle, StartRequested, Recording, StopRequested, Ready, Saving };
    AudioRecorder() = default;                    // C++ to provide normal constructor
    AudioRecorder(const AudioRecorder&) = delete; // Prevent copying of recorder
    AudioRecorder& operator=(const AudioRecorder&) = delete;

    void prepare(double sampleRate); // allocate buffer amount, kept on a restart at the same rate
    bool requestStart();             // returned boolean will indicate if request accepted
    bool requestStop();
    void processInput(const float* inputSamples, int sampleCount);

    // Copies the finished clip and its sample rate out of the recorder. Fails if a take is still
    // recording or nothing has been recorded.
    [[nodiscard]] juce::Result copyRecording(std::vector<float>& samples, double& sampleRate);

    RecordingState getState() const noexcept;

    // Returns the finished clip's samples, or null if there's no buffer. Audio thread only, since
    // the recorder writes the clip there.
    [[nodiscard]] const float* clip() const noexcept;

    // Returns the finished clip's length in samples, or 0 while recording or before a clip exists.
    // Audio thread only.
    [[nodiscard]] std::size_t clipLength() const noexcept;

  private:
    // std::atomic allows state to be read across threads and changed
    std::atomic<RecordingState> state{RecordingState::Idle};

    juce::AudioBuffer<float> recordingBuffer; // hold recorded audio in memory

    double recordingSampleRate = 0.0; // store rate, needed to interpret and save recorded sample
    int recordedSampleCount = 0;      // will record how much of buffer contains audio sample
    static constexpr int maximumRecordingSeconds = 60; // capacity limit, can change

    bool captureActive = false; // Used by audio thread
};