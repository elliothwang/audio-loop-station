#include "core/PanLaw.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1.0e-6f;

} // namespace

TEST_CASE("centre pan is -3 dB on each side") {
    const als::PanGains gains = als::panGains(0.0f);

    CHECK_THAT(gains.left, WithinAbs(std::sqrt(0.5f), kTolerance));
    CHECK_THAT(gains.right, WithinAbs(std::sqrt(0.5f), kTolerance));
}

TEST_CASE("full pan sends a track to one side only") {
    const als::PanGains left = als::panGains(-1.0f);
    CHECK_THAT(left.left, WithinAbs(1.0f, kTolerance));
    CHECK_THAT(left.right, WithinAbs(0.0f, kTolerance));

    const als::PanGains right = als::panGains(1.0f);
    CHECK_THAT(right.left, WithinAbs(0.0f, kTolerance));
    CHECK_THAT(right.right, WithinAbs(1.0f, kTolerance));
}

TEST_CASE("pan keeps a track's power the same across the range") {
    // The knob's own step, so every position it can reach is checked.
    for (int step = -100; step <= 100; ++step) {
        const als::PanGains gains = als::panGains(static_cast<float>(step) / 100.0f);
        CHECK_THAT(gains.left * gains.left + gains.right * gains.right,
                   WithinAbs(1.0f, kTolerance));
    }
}

TEST_CASE("pan moves a track toward the side it's turned to") {
    const als::PanGains slightlyLeft = als::panGains(-0.25f);
    CHECK(slightlyLeft.left > slightlyLeft.right);

    const als::PanGains slightlyRight = als::panGains(0.25f);
    CHECK(slightlyRight.right > slightlyRight.left);
}

TEST_CASE("pan outside the range is clamped, and NaN is centre") {
    const als::PanGains beyondLeft = als::panGains(-3.0f);
    CHECK_THAT(beyondLeft.left, WithinAbs(1.0f, kTolerance));
    CHECK_THAT(beyondLeft.right, WithinAbs(0.0f, kTolerance));

    const als::PanGains beyondRight = als::panGains(3.0f);
    CHECK_THAT(beyondRight.left, WithinAbs(0.0f, kTolerance));
    CHECK_THAT(beyondRight.right, WithinAbs(1.0f, kTolerance));

    const als::PanGains notANumber = als::panGains(std::numeric_limits<float>::quiet_NaN());
    CHECK_THAT(notANumber.left, WithinAbs(std::sqrt(0.5f), kTolerance));
    CHECK_THAT(notANumber.right, WithinAbs(std::sqrt(0.5f), kTolerance));
}
