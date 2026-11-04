#include "LoopPlayback.h"

#include <algorithm>
#include <cassert>

namespace als {

float edgeGain(std::size_t index, std::size_t length, std::size_t fadeLength) noexcept {
    assert(index < length && "index past the end of the loop");
    const std::size_t fade = std::min(fadeLength, length / 2);
    const std::size_t fromEdge = std::min(index, length - 1 - index);
    if (fromEdge >= fade) {
        return 1.0f;
    }
    return static_cast<float>(fromEdge) / static_cast<float>(fade);
}

std::size_t playLooped(const float* loop, std::size_t length, std::size_t position,
                       std::size_t fadeLength, float* output, std::size_t count) noexcept {
    assert(length > 0 && position < length && "position must be inside a non-empty loop");
    for (std::size_t i = 0; i < count; ++i) {
        output[i] = loop[position] * edgeGain(position, length, fadeLength);
        position = position + 1 == length ? 0 : position + 1;
    }
    return position;
}

} // namespace als
