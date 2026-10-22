#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace als {

// A loop's place on the SampleClock timeline.
struct LoopTiming {
    std::int64_t origin = 0; // downbeat, set by the first RECORD press
    std::int64_t length = 0;
};

// Returns nullopt if the take is shorter than minimumLength.
[[nodiscard]] std::optional<LoopTiming> loopFromFirstTake(std::int64_t start, std::int64_t stop,
                                                          std::int64_t minimumLength) noexcept;

[[nodiscard]] std::int64_t loopPosition(const LoopTiming& loop, std::int64_t time) noexcept;

// Positive slip plays the track later, whether or not it's reversed.
[[nodiscard]] std::size_t trackReadIndex(std::int64_t position, std::int64_t length,
                                         std::int64_t slip, bool reversed) noexcept;

// Input arrives `latency` samples late, so it's written that far back.
[[nodiscard]] std::size_t trackWriteIndex(std::int64_t position, std::int64_t length,
                                          std::int64_t latency) noexcept;

} // namespace als
