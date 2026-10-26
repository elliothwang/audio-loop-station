#include "core/LoopTiming.h"
#include "core/SampleClock.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <vector>

namespace {

constexpr std::int64_t kLoopLength = 10;

// buffer[i] == i, so each played sample names its index.
std::vector<std::int64_t> ramp(std::int64_t length) {
    std::vector<std::int64_t> buffer(static_cast<std::size_t>(length));
    std::iota(buffer.begin(), buffer.end(), 0);
    return buffer;
}

als::LoopTiming recordFirstTake(als::SampleClock& clock, std::int64_t length) {
    clock.advance(37); // so the downbeat isn't at 0
    const std::int64_t start = clock.now();
    clock.advance(length);
    const std::optional<als::LoopTiming> loop = als::loopFromFirstTake(start, clock.now(), 1);
    REQUIRE(loop);
    return *loop;
}

// Plays the buffer block by block, like the audio callback.
std::vector<std::int64_t> play(als::SampleClock& clock, const als::LoopTiming& loop,
                               const std::vector<std::int64_t>& buffer, std::int64_t blockSize,
                               std::int64_t sampleCount, std::int64_t slip, bool reversed) {
    REQUIRE(blockSize > 0);
    std::vector<std::int64_t> output;
    while (static_cast<std::int64_t>(output.size()) < sampleCount) {
        const std::int64_t start = als::loopPosition(loop, clock.now());
        for (std::int64_t i = 0; i < blockSize; ++i) {
            output.push_back(
                buffer.at(als::trackReadIndex(start + i, loop.length, slip, reversed)));
        }
        clock.advance(blockSize);
    }
    output.resize(static_cast<std::size_t>(sampleCount));
    return output;
}

} // namespace

TEST_CASE("first take sets origin and length", "[loop_timing]") {
    const std::optional<als::LoopTiming> loop = als::loopFromFirstTake(1000, 1356, 1);
    REQUIRE(loop);
    REQUIRE(loop->origin == 1000);
    REQUIRE(loop->length == 356);
}

TEST_CASE("short, empty, and backward takes are discarded", "[loop_timing]") {
    REQUIRE_FALSE(als::loopFromFirstTake(1000, 1099, 100));
    REQUIRE(als::loopFromFirstTake(1000, 1100, 100));
    REQUIRE_FALSE(als::loopFromFirstTake(1000, 1000, 0));
    REQUIRE_FALSE(als::loopFromFirstTake(1000, 900, 0));
}

TEST_CASE("playback starts at 0 when the first take stops", "[loop_timing]") {
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
    REQUIRE(als::loopPosition(loop, clock.now()) == 0);
}

TEST_CASE("loop position wraps in both directions", "[loop_timing]") {
    const als::LoopTiming loop{1000, kLoopLength};
    REQUIRE(als::loopPosition(loop, 1000) == 0);
    REQUIRE(als::loopPosition(loop, 1009) == 9);
    REQUIRE(als::loopPosition(loop, 1010) == 0);
    REQUIRE(als::loopPosition(loop, 1023) == 3);
    REQUIRE(als::loopPosition(loop, 999) == 9);
}

TEST_CASE("forward playback repeats across block boundaries", "[loop_timing]") {
    for (const std::int64_t blockSize : {1, 3, 7, 13}) {
        als::SampleClock clock;
        const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
        const std::vector<std::int64_t> output =
            play(clock, loop, ramp(kLoopLength), blockSize, 3 * kLoopLength, 0, false);
        for (std::size_t i = 0; i < output.size(); ++i) {
            REQUIRE(output[i] == static_cast<std::int64_t>(i) % kLoopLength);
        }
    }
}

TEST_CASE("reverse playback restarts on the boundary", "[loop_timing]") {
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
    const std::vector<std::int64_t> expected = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
                                                9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    REQUIRE(play(clock, loop, ramp(kLoopLength), 7, 2 * kLoopLength, 0, true) == expected);
}

TEST_CASE("positive slip plays later, forward and reversed", "[loop_timing]") {
    REQUIRE(als::trackReadIndex(3, kLoopLength, 3, false) == 0);
    REQUIRE(als::trackReadIndex(0, kLoopLength, -3, false) == 3);
    REQUIRE(als::trackReadIndex(0, kLoopLength, 0, true) == 9);
    REQUIRE(als::trackReadIndex(3, kLoopLength, 3, true) == 9);
}

TEST_CASE("slip wraps past a full loop", "[loop_timing]") {
    for (std::int64_t position = 0; position < kLoopLength; ++position) {
        REQUIRE(als::trackReadIndex(position, kLoopLength, 23, false) ==
                als::trackReadIndex(position, kLoopLength, 3, false));
        REQUIRE(als::trackReadIndex(position, kLoopLength, -23, true) ==
                als::trackReadIndex(position, kLoopLength, -3, true));
    }
}

TEST_CASE("every sample plays once per pass", "[loop_timing]") {
    for (const std::int64_t slip : {-13, -1, 0, 4, 17}) {
        for (const bool reversed : {false, true}) {
            std::vector<int> plays(static_cast<std::size_t>(kLoopLength), 0);
            for (std::int64_t position = 0; position < kLoopLength; ++position) {
                ++plays.at(als::trackReadIndex(position, kLoopLength, slip, reversed));
            }
            REQUIRE(plays == std::vector<int>(static_cast<std::size_t>(kLoopLength), 1));
        }
    }
}

TEST_CASE("milliseconds round to the nearest sample", "[loop_timing]") {
    REQUIRE(als::millisecondsToSamples(0, 48000.0) == 0);
    REQUIRE(als::millisecondsToSamples(10, 48000.0) == 480);
    REQUIRE(als::millisecondsToSamples(-10, 48000.0) == -480);
    REQUIRE(als::millisecondsToSamples(1, 44100.0) == 44);    // 44.1
    REQUIRE(als::millisecondsToSamples(5, 44100.0) == 221);   // 220.5, away from zero
    REQUIRE(als::millisecondsToSamples(-5, 44100.0) == -221); // same rounding when earlier
}

TEST_CASE("a slip set in milliseconds plays that much later", "[loop_timing]") {
    constexpr double kSampleRate = 48000.0;
    constexpr std::int64_t kOneSecondLoop = 48000;
    const std::int64_t slip = als::millisecondsToSamples(10, kSampleRate);
    REQUIRE(als::trackReadIndex(480, kOneSecondLoop, slip, false) == 0);
    REQUIRE(als::trackReadIndex(0, kOneSecondLoop, -slip, false) == 480);
}

TEST_CASE("write index shifts back by latency", "[loop_timing]") {
    REQUIRE(als::trackWriteIndex(3, kLoopLength, 0) == 3);
    REQUIRE(als::trackWriteIndex(3, kLoopLength, 3) == 0);
    REQUIRE(als::trackWriteIndex(0, kLoopLength, 3) == 7);
}

TEST_CASE("latency-compensated recording lines up with playback", "[loop_timing]") {
    constexpr std::int64_t kLatency = 4;
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
    std::vector<std::int64_t> track(static_cast<std::size_t>(kLoopLength), -1);
    for (std::int64_t i = 0; i < kLoopLength + kLatency; ++i) {
        // What the performer played in time with playback, kLatency samples ago.
        const std::int64_t played = als::loopPosition(loop, clock.now() - kLatency);
        const std::int64_t position = als::loopPosition(loop, clock.now());
        track.at(als::trackWriteIndex(position, kLoopLength, kLatency)) = played;
        clock.advance(1);
    }
    REQUIRE(track == ramp(kLoopLength));
}

TEST_CASE("loop position stays exact over ten minutes", "[loop_timing]") {
    constexpr std::int64_t kBlockSize = 512;
    constexpr std::int64_t kLongLoop = 96001; // not a multiple of the block size
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLongLoop);
    std::int64_t expected = 0;
    for (std::int64_t block = 0; block < 10 * 60 * 48000 / kBlockSize; ++block) {
        clock.advance(kBlockSize);
        expected = (expected + kBlockSize) % kLongLoop;
        REQUIRE(als::loopPosition(loop, clock.now()) == expected);
    }
}
