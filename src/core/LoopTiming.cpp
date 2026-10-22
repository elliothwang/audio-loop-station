#include "LoopTiming.h"

#include <cassert>

namespace als {

namespace {

// Modulo that is never negative, so times before the origin and negative offsets wrap to the end
// of the loop. Returns 0 for a length that is not positive, which only a caller bug can cause.
std::int64_t wrap(std::int64_t value, std::int64_t length) noexcept {
    assert(length > 0 && "loop length must be positive");
    if (length <= 0) {
        return 0;
    }
    return ((value % length) + length) % length;
}

} // namespace

std::optional<LoopTiming> loopFromFirstTake(std::int64_t start, std::int64_t stop,
                                            std::int64_t minimumLength) noexcept {
    const std::int64_t length = stop - start;
    if (length <= 0 || length < minimumLength) {
        return std::nullopt;
    }
    return LoopTiming{start, length};
}

std::int64_t loopPosition(const LoopTiming& loop, std::int64_t time) noexcept {
    return wrap(time - loop.origin, loop.length);
}

std::size_t trackReadIndex(std::int64_t position, std::int64_t length, std::int64_t slip,
                           bool reversed) noexcept {
    const std::int64_t shifted = wrap(position - slip, length);
    // Mirror after the slip shift, so positive slip sounds later in both directions. The outer
    // wrap is a no-op for a valid length and keeps the index in range for an invalid one.
    return static_cast<std::size_t>(reversed ? wrap(length - 1 - shifted, length) : shifted);
}

std::size_t trackWriteIndex(std::int64_t position, std::int64_t length,
                            std::int64_t latency) noexcept {
    assert(latency >= 0 && "latency must not be negative");
    return static_cast<std::size_t>(wrap(position - latency, length));
}

} // namespace als
