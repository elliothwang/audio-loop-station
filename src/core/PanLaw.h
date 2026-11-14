#pragma once

// The pan law the mixer uses to split each track between the left and right outputs.

namespace als {

// How much of a track goes to each output, from 0 to 1.
struct PanGains {
    float left = 0.0f;
    float right = 0.0f;
};

// Returns the constant power gains for `pan`, from -1 for full left to 1 for full right. Centre
// is -3 dB on each side, so a track sounds as loud wherever it's panned. Values outside the range
// are clamped, and NaN is treated as centre.
[[nodiscard]] PanGains panGains(float pan) noexcept;

} // namespace als
