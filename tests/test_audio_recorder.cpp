#include "AudioRecorder.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace {

using Tracks = std::array<std::vector<float>, als::kTrackCount>;
using Starts = std::array<std::int64_t, als::kTrackCount>;

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

    Tracks samples;
    double sampleRate = 0.0;
    REQUIRE(recorder.copyTracks(samples, sampleRate));
    REQUIRE(samples == Tracks{kInput, {}, {}, {}});
    REQUIRE(sampleRate == kSampleRate);
    REQUIRE(recorder.getState() == AudioRecorder::RecordingState::Ready);
}

TEST_CASE("there's no clip while recording or before one exists", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    Tracks samples;
    double sampleRate = 0.0;
    REQUIRE(recorder.clipLength() == 0);
    REQUIRE_FALSE(recorder.copyTracks(samples, sampleRate));

    REQUIRE(recorder.requestStart());
    recorder.processInput(kInput.data(), static_cast<int>(kInput.size()));
    REQUIRE(recorder.clipLength() == 0);
    REQUIRE_FALSE(recorder.copyTracks(samples, sampleRate));
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

TEST_CASE("loaded tracks replace every track at the next block", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    recordClip(recorder);

    // Track 1 is left empty, so the recorded clip there has to be cleared.
    Tracks samples;
    samples[1] = {0.5f, 0.5f, -0.5f, -0.5f};
    samples[3] = {0.125f};
    const Starts starts{0, 4, 0, 5};
    REQUIRE(recorder.loadTracks(samples, starts, 6, kSampleRate).wasOk());
    REQUIRE(recorder.clipLength() == 0); // not playable until the audio thread copies it in
    REQUIRE_FALSE(recorder.requestStart());

    const auto loaded = recorder.processInput(nullptr, 0);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->loopLength == 6);
    REQUIRE(loaded->starts == starts);
    REQUIRE(loaded->hasAudio == std::array<bool, als::kTrackCount>{false, true, false, true});
    REQUIRE(recorder.getState() == AudioRecorder::RecordingState::Ready);

    Tracks copied;
    double sampleRate = 0.0;
    REQUIRE(recorder.copyTracks(copied, sampleRate));
    REQUIRE(copied == samples);
    REQUIRE_FALSE(recorder.processInput(nullptr, 0).has_value()); // a load applies only once
}

TEST_CASE("tracks that don't fit aren't loaded", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    const Tracks oneTrack{kInput, {}, {}, {}};
    const Starts noStarts{};
    const auto inputLength = static_cast<std::int64_t>(kInput.size());

    REQUIRE(recorder.loadTracks(oneTrack, noStarts, inputLength, 44100.0).failed());
    REQUIRE(recorder.loadTracks(Tracks{}, noStarts, inputLength, kSampleRate).failed());
    REQUIRE(recorder.loadTracks(oneTrack, noStarts, inputLength - 1, kSampleRate).failed());
    REQUIRE(recorder.loadTracks(oneTrack, Starts{inputLength, 0, 0, 0}, inputLength, kSampleRate)
                .failed());
    REQUIRE(recorder.loadTracks(oneTrack, noStarts, 61 * 48000, kSampleRate).failed());

    REQUIRE(recorder.requestStart());
    REQUIRE(recorder.loadTracks(oneTrack, noStarts, inputLength, kSampleRate).failed());
}

TEST_CASE("each finished take or load counts as a new clip", "[audio_recorder]") {
    AudioRecorder recorder;
    recorder.prepare(kSampleRate);
    REQUIRE(recorder.finishedClips() == 0);
    recordClip(recorder);
    REQUIRE(recorder.finishedClips() == 1);

    Tracks samples;
    double sampleRate = 0.0;
    REQUIRE(recorder.copyTracks(samples, sampleRate));
    REQUIRE(recorder.finishedClips() == 1); // copying isn't a new clip

    REQUIRE(recorder
                .loadTracks(Tracks{kInput, {}, {}, {}}, Starts{},
                            static_cast<std::int64_t>(kInput.size()), kSampleRate)
                .wasOk());
    recorder.processInput(nullptr, 0);
    REQUIRE(recorder.finishedClips() == 2);
}
