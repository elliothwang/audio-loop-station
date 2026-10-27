#include "LoopTiming.h"

#include <cassert>
#include <cmath>

namespace als {

namespace {

// Like %, but never negative.
std::int64_t wrap(std::int64_t value, std::int64_t length) noexcept {
    assert(length > 0 && "loop length must be positive");
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
    return static_cast<std::size_t>(reversed ? length - 1 - shifted : shifted);
}

std::int64_t millisecondsToSamples(std::int64_t milliseconds, double sampleRate) noexcept {
    assert(sampleRate > 0 && "sample rate must be positive");
    return std::llround(static_cast<double>(milliseconds) * sampleRate / 1000.0);
}

std::size_t trackWriteIndex(std::int64_t position, std::int64_t length,
                            std::int64_t latency) noexcept {
    return static_cast<std::size_t>(wrap(position - latency, length));
}

} // namespace als
