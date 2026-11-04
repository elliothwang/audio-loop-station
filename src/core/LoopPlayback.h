#pragma once

// Reads a loop's samples into an output block, wrapping from the end back to the start so the
// loop repeats without a gap. Pure functions, so the wrap and the fades are testable without audio.

#include <cstddef>

namespace als {

// Returns the gain for sample `index` of a `length` sample loop, ramping from 0 to 1 over
// `fadeLength` samples at both ends so the wrap doesn't click. Loops shorter than two fades are
// faded over half their length.
[[nodiscard]] float edgeGain(std::size_t index, std::size_t length,
                             std::size_t fadeLength) noexcept;

// Copies `count` samples of `loop` into `output`, starting at `position` and wrapping at
// `length`, with edgeGain() applied. Returns the position after the last sample copied.
[[nodiscard]] std::size_t playLooped(const float* loop, std::size_t length, std::size_t position,
                                     std::size_t fadeLength, float* output,
                                     std::size_t count) noexcept;

} // namespace als
