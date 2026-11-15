#include "PanLaw.h"

#include <algorithm>
#include <cmath>

namespace als {

namespace {

constexpr float kQuarterPi = 0.785398163f; // [rad]

} // namespace

PanGains panGains(float pan) noexcept {
    const float clamped = std::isnan(pan) ? 0.0f : std::clamp(pan, -1.0f, 1.0f);

    // Sweeps a quarter circle, so left squared plus right squared is always 1.
    const float angle = (clamped + 1.0f) * kQuarterPi;
    return {std::cos(angle), std::sin(angle)};
}

} // namespace als
