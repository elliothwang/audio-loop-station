#include "core/SampleClock.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("a new clock starts at zero", "[sample_clock]") {
    const als::SampleClock clock;
    REQUIRE(clock.now() == 0);
}

TEST_CASE("the clock advances by each block's actual sample count", "[sample_clock]") {
    als::SampleClock clock;
    clock.advance(512);
    REQUIRE(clock.now() == 512);
    clock.advance(480);
    REQUIRE(clock.now() == 992);
    clock.advance(7);
    REQUIRE(clock.now() == 999);
}

TEST_CASE("an empty block leaves the clock unchanged", "[sample_clock]") {
    als::SampleClock clock;
    clock.advance(256);
    clock.advance(0);
    REQUIRE(clock.now() == 256);
}

TEST_CASE("ten minutes of 48 kHz blocks counts every sample exactly", "[sample_clock]") {
    constexpr std::int64_t kSampleRate = 48000; // [samples/s]
    constexpr std::int64_t kBlockSize = 512;    // [samples]
    constexpr std::int64_t kSeconds = 600;
    constexpr std::int64_t kBlockCount = kSeconds * kSampleRate / kBlockSize; // divides exactly

    als::SampleClock clock;
    for (std::int64_t block = 0; block < kBlockCount; ++block) {
        clock.advance(kBlockSize);
    }
    REQUIRE(clock.now() == kSeconds * kSampleRate);
}
