#include "core/LoopTiming.h"
#include "core/SampleClock.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <vector>

namespace {

constexpr std::int64_t kLoopLength = 10; // loop length used by most tests [samples]

// Test buffer whose sample at index i holds i, so each sample played names the index it came from.
std::vector<std::int64_t> makeRampBuffer(std::int64_t length) {
    std::vector<std::int64_t> buffer(static_cast<std::size_t>(length));
    std::iota(buffer.begin(), buffer.end(), 0);
    return buffer;
}

// Records a first take of the given length. Starts partway into the stream so the downbeat is not
// at clock time 0.
als::LoopTiming recordFirstTake(als::SampleClock& clock, std::int64_t length) {
    clock.advance(37);
    const std::int64_t start = clock.now();
    clock.advance(length);
    const std::optional<als::LoopTiming> loop = als::loopFromFirstTake(start, clock.now(), 1);
    REQUIRE(loop.has_value());
    return *loop;
}

// Plays the buffer the way the audio callback will: map each sample of a block from the block's
// start position, then advance the clock by the block.
std::vector<std::int64_t> play(als::SampleClock& clock, const als::LoopTiming& loop,
                               const std::vector<std::int64_t>& buffer, std::int64_t blockSize,
                               std::int64_t sampleCount, std::int64_t slip, bool reversed) {
    REQUIRE(blockSize > 0);
    std::vector<std::int64_t> output;
    while (static_cast<std::int64_t>(output.size()) < sampleCount) {
        const std::int64_t blockStart = als::loopPosition(loop, clock.now());
        for (std::int64_t i = 0; i < blockSize; ++i) {
            output.push_back(
                buffer.at(als::trackReadIndex(blockStart + i, loop.length, slip, reversed)));
        }
        clock.advance(blockSize);
    }
    output.resize(static_cast<std::size_t>(sampleCount));
    return output;
}

} // namespace

TEST_CASE("the first take sets the downbeat and the loop length", "[loop_timing]") {
    const std::optional<als::LoopTiming> loop = als::loopFromFirstTake(1000, 1356, 1);
    REQUIRE(loop.has_value());
    REQUIRE(loop->origin == 1000);
    REQUIRE(loop->length == 356);
}

TEST_CASE("a take shorter than the minimum is discarded", "[loop_timing]") {
    REQUIRE_FALSE(als::loopFromFirstTake(1000, 1099, 100).has_value());
    REQUIRE(als::loopFromFirstTake(1000, 1100, 100).has_value());
}

TEST_CASE("an empty or backward take is discarded", "[loop_timing]") {
    REQUIRE_FALSE(als::loopFromFirstTake(1000, 1000, 0).has_value());
    REQUIRE_FALSE(als::loopFromFirstTake(1000, 900, 0).has_value());
}

TEST_CASE("playback restarts at the downbeat the moment the first take stops", "[loop_timing]") {
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
    REQUIRE(als::loopPosition(loop, clock.now()) == 0);
}

TEST_CASE("loop position wraps at the loop boundary", "[loop_timing]") {
    const als::LoopTiming loop{1000, kLoopLength};
    REQUIRE(als::loopPosition(loop, 1000) == 0);
    REQUIRE(als::loopPosition(loop, 1009) == 9);
    REQUIRE(als::loopPosition(loop, 1010) == 0);
    REQUIRE(als::loopPosition(loop, 1023) == 3);
}

TEST_CASE("times before the downbeat wrap to the end of the loop", "[loop_timing]") {
    const als::LoopTiming loop{1000, kLoopLength};
    REQUIRE(als::loopPosition(loop, 999) == 9);
    REQUIRE(als::loopPosition(loop, 990) == 0);
}

TEST_CASE("forward playback repeats the buffer across block boundaries", "[loop_timing]") {
    const std::vector<std::int64_t> buffer = makeRampBuffer(kLoopLength);

    // Block sizes that do not divide the loop length, so blocks straddle the boundary.
    for (const std::int64_t blockSize : {1, 3, 7, 13}) {
        als::SampleClock clock;
        const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
        const std::vector<std::int64_t> output =
            play(clock, loop, buffer, blockSize, 3 * kLoopLength, 0, false);
        for (std::size_t i = 0; i < output.size(); ++i) {
            REQUIRE(output[i] == static_cast<std::int64_t>(i) % kLoopLength);
        }
    }
}

TEST_CASE("reverse playback reads from the end and restarts on the boundary", "[loop_timing]") {
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);
    const std::vector<std::int64_t> output =
        play(clock, loop, makeRampBuffer(kLoopLength), 7, 2 * kLoopLength, 0, true);
    const std::vector<std::int64_t> expected = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
                                                9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    REQUIRE(output == expected);
}

TEST_CASE("positive slip makes a forward track sound later", "[loop_timing]") {
    // The first sample plays at loop position 3 instead of 0.
    REQUIRE(als::trackReadIndex(3, kLoopLength, 3, false) == 0);
    REQUIRE(als::trackReadIndex(0, kLoopLength, 3, false) == 7);
    REQUIRE(als::trackReadIndex(2, kLoopLength, 3, false) == 9);
}

TEST_CASE("negative slip makes a forward track sound earlier", "[loop_timing]") {
    REQUIRE(als::trackReadIndex(0, kLoopLength, -3, false) == 3);
    REQUIRE(als::trackReadIndex(7, kLoopLength, -3, false) == 0);
}

TEST_CASE("positive slip makes a reversed track sound later too", "[loop_timing]") {
    // Unslipped, the reversed track's first sound (buffer index 9) plays at position 0.
    REQUIRE(als::trackReadIndex(0, kLoopLength, 0, true) == 9);
    // Slipped by +3, that same sound plays at position 3.
    REQUIRE(als::trackReadIndex(3, kLoopLength, 3, true) == 9);
    REQUIRE(als::trackReadIndex(2, kLoopLength, 3, true) == 0);
}

TEST_CASE("slip of a whole loop or more wraps around", "[loop_timing]") {
    for (std::int64_t position = 0; position < kLoopLength; ++position) {
        const std::size_t unslipped = als::trackReadIndex(position, kLoopLength, 0, false);
        REQUIRE(als::trackReadIndex(position, kLoopLength, 10, false) == unslipped);
        REQUIRE(als::trackReadIndex(position, kLoopLength, -10, false) == unslipped);
        REQUIRE(als::trackReadIndex(position, kLoopLength, 23, false) ==
                als::trackReadIndex(position, kLoopLength, 3, false));
        REQUIRE(als::trackReadIndex(position, kLoopLength, -23, true) ==
                als::trackReadIndex(position, kLoopLength, -3, true));
    }
}

TEST_CASE("read index always lands inside the track", "[loop_timing]") {
    for (std::int64_t position = -25; position <= 25; ++position) {
        for (std::int64_t slip = -25; slip <= 25; ++slip) {
            for (const bool reversed : {false, true}) {
                REQUIRE(als::trackReadIndex(position, kLoopLength, slip, reversed) <
                        static_cast<std::size_t>(kLoopLength));
            }
        }
    }
}

TEST_CASE("slipped and reversed tracks play every sample once per pass", "[loop_timing]") {
    for (const std::int64_t slip : {-13, -1, 0, 4, 17}) {
        for (const bool reversed : {false, true}) {
            std::vector<int> timesRead(static_cast<std::size_t>(kLoopLength), 0);
            for (std::int64_t position = 0; position < kLoopLength; ++position) {
                ++timesRead.at(als::trackReadIndex(position, kLoopLength, slip, reversed));
            }
            for (const int count : timesRead) {
                REQUIRE(count == 1);
            }
        }
    }
}

TEST_CASE("write index moves input back by the round-trip latency", "[loop_timing]") {
    REQUIRE(als::trackWriteIndex(3, kLoopLength, 0) == 3);
    REQUIRE(als::trackWriteIndex(3, kLoopLength, 3) == 0);
    REQUIRE(als::trackWriteIndex(0, kLoopLength, 3) == 7);
    REQUIRE(als::trackWriteIndex(5, kLoopLength, 23) == 2);
}

TEST_CASE("latency-compensated recording lines up with what the performer heard", "[loop_timing]") {
    constexpr std::int64_t kLatency = 4;   // [samples]
    constexpr std::int64_t kBlockSize = 3; // [samples]
    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLoopLength);

    // Record more than one pass plus the latency, so every index is written at least once.
    std::vector<std::int64_t> track(static_cast<std::size_t>(kLoopLength), -1);
    for (std::int64_t block = 0; block < 5; ++block) {
        for (std::int64_t i = 0; i < kBlockSize; ++i) {
            const std::int64_t time = clock.now() + i;
            // The performer plays in time with what they hear, so the sample arriving now was
            // played against the loop position from kLatency samples ago.
            const std::int64_t played = als::loopPosition(loop, time - kLatency);
            track.at(als::trackWriteIndex(als::loopPosition(loop, time), kLoopLength, kLatency)) =
                played;
        }
        clock.advance(kBlockSize);
    }
    REQUIRE(track == makeRampBuffer(kLoopLength));
}

TEST_CASE("ten minutes of blocks keeps the loop position exact", "[loop_timing]") {
    constexpr std::int64_t kSampleRate = 48000; // [samples/s]
    constexpr std::int64_t kBlockSize = 512;    // [samples]
    constexpr std::int64_t kLength = 96001;     // just over 2 s, not a multiple of the block size
    constexpr std::int64_t kBlockCount = 10 * 60 * kSampleRate / kBlockSize;

    als::SampleClock clock;
    const als::LoopTiming loop = recordFirstTake(clock, kLength);
    std::int64_t previous = als::loopPosition(loop, clock.now());
    for (std::int64_t block = 0; block < kBlockCount; ++block) {
        clock.advance(kBlockSize);
        const std::int64_t position = als::loopPosition(loop, clock.now());
        REQUIRE(position == (previous + kBlockSize) % kLength);
        previous = position;
    }
    REQUIRE(previous == (clock.now() - loop.origin) % kLength);
}
