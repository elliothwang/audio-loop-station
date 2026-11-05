#include "AudioRecorder.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
const std::vector<float> kInput{0.25f, -0.5f, 0.75f};

// Records kInput as one finished clip.
void recordClip(AudioRecorder& recorder) {
    REQUIRE(recorder.requestStart());
    recorder.processInput(kInput.data(), static_cast<int>(kInput.size()));
    REQUIRE(recorder.requestStop());
    recorder.processInput(nullptr, 0); // the audio thread finishes the stop
    REQUIRE(recorder.getState() == AudioRecorder::RecordingState::Ready);
}

} // namespace

TEST_CASE("a finished clip can be copied and played", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    recordClip(recorder);

    REQUIRE(recorder.clipLength() == kInput.size());
    REQUIRE(std::vector<float>(recorder.clip(), recorder.clip() + kInput.size()) == kInput);

    std::vector<float> samples;
    double sampleRate = 0.0;
    REQUIRE(recorder.copyRecording(samples, sampleRate).wasOk());
    REQUIRE(samples == kInput);
    REQUIRE(sampleRate == kSampleRate);
    REQUIRE(recorder.getState() == AudioRecorder::RecordingState::Ready);
}

TEST_CASE("there's no clip while recording or before one exists", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    std::vector<float> samples;
    double sampleRate = 0.0;
    REQUIRE(recorder.clipLength() == 0);
    REQUIRE(recorder.copyRecording(samples, sampleRate).failed());

    REQUIRE(recorder.requestStart());
    recorder.processInput(kInput.data(), static_cast<int>(kInput.size()));
    REQUIRE(recorder.clipLength() == 0);
    REQUIRE(recorder.copyRecording(samples, sampleRate).failed());
}

TEST_CASE("a restart at the same rate keeps the clip", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    recordClip(recorder);

    recorder.prepare(kSampleRate);
    REQUIRE(recorder.clipLength() == kInput.size());

    recorder.prepare(44100.0); // a new rate no longer matches the clip
    REQUIRE(recorder.clipLength() == 0);
}

TEST_CASE("a loaded clip replaces the recording at the next block", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    recordClip(recorder);

    const std::vector<float> loaded{0.5f, 0.5f, -0.5f, -0.5f};
    REQUIRE(recorder.loadClip(loaded, kSampleRate).wasOk());
    REQUIRE(recorder.clipLength() == 0); // not playable until the audio thread copies it in
    REQUIRE_FALSE(recorder.requestStart());

    recorder.processInput(nullptr, 0);
    REQUIRE(recorder.getState() == AudioRecorder::RecordingState::Ready);
    REQUIRE(std::vector<float>(recorder.clip(), recorder.clip() + recorder.clipLength()) == loaded);
}

TEST_CASE("a clip that doesn't fit isn't loaded", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    REQUIRE(recorder.loadClip(kInput, 44100.0).failed());
    REQUIRE(recorder.loadClip({}, kSampleRate).failed());
    REQUIRE(recorder.loadClip(std::vector<float>(61 * 48000), kSampleRate).failed());

    REQUIRE(recorder.requestStart());
    REQUIRE(recorder.loadClip(kInput, kSampleRate).failed());
}

TEST_CASE("each finished take or load counts as a new clip", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    REQUIRE(recorder.finishedClips() == 0);
    recordClip(recorder);
    REQUIRE(recorder.finishedClips() == 1);

    std::vector<float> samples;
    double sampleRate = 0.0;
    REQUIRE(recorder.copyRecording(samples, sampleRate).wasOk());
    REQUIRE(recorder.finishedClips() == 1); // copying isn't a new clip

    REQUIRE(recorder.loadClip(kInput, kSampleRate).wasOk());
    recorder.processInput(nullptr, 0);
    REQUIRE(recorder.finishedClips() == 2);
}
