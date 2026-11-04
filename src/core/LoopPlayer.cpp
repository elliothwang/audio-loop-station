#include "LoopPlayer.h"

#include "LoopPlayback.h"

#include <algorithm>
#include <cmath>

namespace als {

void LoopPlayer::prepare(double sampleRate) noexcept {
    const double fade = sampleRate * kFadeMilliseconds / 1000.0;
    fadeLength_ =
        std::isfinite(fade) && fade > 0.0 ? static_cast<std::size_t>(std::lround(fade)) : 0;
}

void LoopPlayer::play() noexcept {
    restart_.store(true);
    playing_.store(true);
}

void LoopPlayer::stop() noexcept {
    playing_.store(false);
}

bool LoopPlayer::playing() const noexcept {
    return playing_.load();
}

void LoopPlayer::process(const float* loop, std::size_t length, float* left, float* right,
                         std::size_t count) noexcept {
    publishedLength_.store(static_cast<std::int64_t>(length), std::memory_order_relaxed);

    // A flag rather than the stopped state, so Stop then Play inside one block still restarts.
    if (restart_.exchange(false)) {
        position_ = 0;
    }
    if (!playing_.load() || loop == nullptr || length == 0 || left == nullptr) {
        position_ = 0;
        publishedPosition_.store(0, std::memory_order_relaxed);
        return;
    }
    position_ = playLooped(loop, length, position_, fadeLength_, left, count);
    if (right != nullptr) {
        std::copy_n(left, count, right);
    }
    publishedPosition_.store(static_cast<std::int64_t>(position_), std::memory_order_relaxed);
}

std::int64_t LoopPlayer::position() const noexcept {
    return publishedPosition_.load(std::memory_order_relaxed);
}

std::int64_t LoopPlayer::length() const noexcept {
    return publishedLength_.load(std::memory_order_relaxed);
}

} // namespace als
