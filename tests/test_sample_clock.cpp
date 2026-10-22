#include "core/SampleClock.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("clock starts at 0", "[sample_clock]") {
    const als::SampleClock clock;
    REQUIRE(clock.now() == 0);
}

TEST_CASE("clock advances by each block's sample count", "[sample_clock]") {
    als::SampleClock clock;
    clock.advance(512);
    clock.advance(480);
    clock.advance(0);
    clock.advance(7);
    REQUIRE(clock.now() == 999);
}

TEST_CASE("clock counts ten minutes of blocks exactly", "[sample_clock]") {
    als::SampleClock clock;
    for (std::int64_t block = 0; block < 10 * 60 * 48000 / 512; ++block) {
        clock.advance(512);
    }
    REQUIRE(clock.now() == 10 * 60 * 48000);
}
