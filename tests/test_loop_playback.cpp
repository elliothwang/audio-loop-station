#include "core/LoopPlayback.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using als::edgeGain;
using als::playLooped;

namespace {

// A loop of ones, so the output shows only the fades.
const std::vector<float> kOnes(10, 1.0f);

} // namespace

TEST_CASE("edge gain fades in and out at the loop's ends", "[loop_playback]") {
    REQUIRE(edgeGain(0, 10, 2) == 0.0f);
    REQUIRE(edgeGain(1, 10, 2) == 0.5f);
    REQUIRE(edgeGain(2, 10, 2) == 1.0f);
    REQUIRE(edgeGain(8, 10, 2) == 0.5f);
    REQUIRE(edgeGain(9, 10, 2) == 0.0f);
}

TEST_CASE("edge gain is flat without a fade", "[loop_playback]") {
    REQUIRE(edgeGain(0, 10, 0) == 1.0f);
    REQUIRE(edgeGain(0, 1, 5) == 1.0f); // too short to fade at all
}

TEST_CASE("a short loop fades over half its length", "[loop_playback]") {
    REQUIRE(edgeGain(0, 4, 100) == 0.0f);
    REQUIRE(edgeGain(1, 4, 100) == 0.5f);
    REQUIRE(edgeGain(2, 4, 100) == 0.5f);
}

TEST_CASE("playback wraps from the end back to the start", "[loop_playback]") {
    const std::vector<float> loop{0.1f, 0.2f, 0.3f, 0.4f};
    std::vector<float> output(6);
    const std::size_t next = playLooped(loop.data(), loop.size(), 2, 0, output.data(), 6);
    REQUIRE(output == std::vector<float>{0.3f, 0.4f, 0.1f, 0.2f, 0.3f, 0.4f});
    REQUIRE(next == 0);
}

TEST_CASE("a block longer than the loop repeats it", "[loop_playback]") {
    std::vector<float> output(25);
    const std::size_t next = playLooped(kOnes.data(), kOnes.size(), 0, 2, output.data(), 25);
    REQUIRE(next == 5);
    // The loop's start and end land on silence each time it wraps.
    REQUIRE(output[0] == 0.0f);
    REQUIRE(output[9] == 0.0f);
    REQUIRE(output[10] == 0.0f);
    REQUIRE(output[20] == 0.0f);
    REQUIRE(output[5] == 1.0f);
}

TEST_CASE("consecutive blocks continue where the last one ended", "[loop_playback]") {
    const std::vector<float> loop{0.1f, 0.2f, 0.3f};
    std::vector<float> first(2);
    std::vector<float> second(2);
    std::size_t position = playLooped(loop.data(), loop.size(), 0, 0, first.data(), 2);
    position = playLooped(loop.data(), loop.size(), position, 0, second.data(), 2);
    REQUIRE(first == std::vector<float>{0.1f, 0.2f});
    REQUIRE(second == std::vector<float>{0.3f, 0.1f});
    REQUIRE(position == 1);
}
