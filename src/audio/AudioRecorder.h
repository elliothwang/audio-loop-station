#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

class AudioRecorder {
  public:
    enum class RecordingState {
        Idle,
        StartRequested,
        Recording,
        StopRequested,
        Ready,
        Saving,
        Loading
    };
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

    // Hands a saved clip to the audio thread, which copies it in as the finished clip at its next
    // block. Fails while recording, or if the clip is empty, too long, or at another sample rate.
    [[nodiscard]] juce::Result loadClip(const std::vector<float>& samples, double sampleRate);

    // Empties the clip at the audio thread's next block. Fails while recording.
    [[nodiscard]] juce::Result clearClip();

    RecordingState getState() const noexcept;

    // Returns how many clips have finished, from takes, loads, and deletes. A change means a new
    // clip, even when the state went back to Ready between two reads.
    [[nodiscard]] std::uint32_t finishedClips() const noexcept;

    // Returns the finished clip's samples, or null if there's no buffer. Audio thread only, since
    // the recorder writes the clip there.
    [[nodiscard]] const float* clip() const noexcept;

    // Returns the finished clip's length in samples, or 0 while recording or before a clip exists.
    // Audio thread only.
    [[nodiscard]] std::size_t clipLength() const noexcept;

    // Buffer capacity in samples. This will be read when audio processing stopped.
    [[nodiscard]] int capacity() const noexcept {
        return recordingBuffer.getNumSamples();
    }

  private:
    // Marks the clip finished and Ready. Audio thread.
    void finishClip() noexcept;

    // Moves to Loading and hands `samples` to the audio thread as the next clip. Fails with
    // `busyMessage` unless the recorder is Idle or Ready.
    juce::Result handOverClip(const std::vector<float>& samples, const juce::String& busyMessage);

    // std::atomic allows state to be read across threads and changed
    std::atomic<RecordingState> state{RecordingState::Idle};

    juce::AudioBuffer<float> recordingBuffer; // hold recorded audio in memory

    double recordingSampleRate = 0.0; // store rate, needed to interpret and save recorded sample
    int recordedSampleCount = 0;      // will record how much of buffer contains audio sample
    static constexpr int maximumRecordingSeconds = 60; // capacity limit, can change

    bool captureActive = false; // Used by audio thread

    // Copies a pending clip into the buffer and finishes it. Audio thread, while Loading.
    void applyPendingClip() noexcept;

    // Set by handOverClip() while Loading, read by the audio thread once clipPending is true.
    std::vector<float> pendingClip;
    std::atomic<bool> clipPending{false};

    // Goes up each time a clip finishes, read through finishedClips().
    std::atomic<std::uint32_t> clipCount{0};
};