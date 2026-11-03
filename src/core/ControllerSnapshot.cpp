#include "ControllerSnapshot.h"

#include <cassert>

namespace als {

void ControllerSnapshot::publish(const LoopController& controller, std::int64_t now) noexcept {
    transport_.store(controller.transport(), std::memory_order_relaxed);
    for (std::size_t i = 0; i < kTrackCount; ++i) {
        tracks_[i].store(controller.track(i), std::memory_order_relaxed);
    }
    now_.store(now, std::memory_order_relaxed);

    std::int64_t length = 0;
    std::int64_t position = 0;
    if (const auto& loop = controller.loop()) {
        length = loop->length;
        position = loopPosition(*loop, now);
    } else if (const auto start = controller.takeStart()) {
        position = now - *start;
    }
    loopLength_.store(length, std::memory_order_relaxed);
    position_.store(position, std::memory_order_relaxed);
}

void ControllerSnapshot::publishResult(Result result) noexcept {
    lastResult_.store(result, std::memory_order_relaxed);
    resultCount_.fetch_add(1, std::memory_order_release);
}

Transport ControllerSnapshot::transport() const noexcept {
    return transport_.load(std::memory_order_relaxed);
}

TrackState ControllerSnapshot::track(std::size_t index) const noexcept {
    assert(index < kTrackCount && "track index out of range");
    return index < kTrackCount ? tracks_[index].load(std::memory_order_relaxed) : TrackState::Empty;
}

std::int64_t ControllerSnapshot::now() const noexcept {
    return now_.load(std::memory_order_relaxed);
}

std::int64_t ControllerSnapshot::loopLength() const noexcept {
    return loopLength_.load(std::memory_order_relaxed);
}

std::int64_t ControllerSnapshot::position() const noexcept {
    return position_.load(std::memory_order_relaxed);
}

Result ControllerSnapshot::lastResult() const noexcept {
    return lastResult_.load(std::memory_order_relaxed);
}

std::uint32_t ControllerSnapshot::resultCount() const noexcept {
    return resultCount_.load(std::memory_order_acquire);
}

} // namespace als
