#pragma once

#include "LoopController.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace als {

// A copy of the controller's state for the UI thread. The audio thread publishes it once per
// block; fields are read independently, so a read can mix values one block apart.
class ControllerSnapshot {
  public:
    void publish(const LoopController& controller, std::int64_t now) noexcept;

    // Each result bumps resultCount, so the UI can show a repeated notice once per press.
    void publishResult(Result result) noexcept;

    [[nodiscard]] Transport transport() const noexcept;
    [[nodiscard]] TrackState track(std::size_t index) const noexcept;
    [[nodiscard]] std::int64_t now() const noexcept;
    [[nodiscard]] std::int64_t loopLength() const noexcept; // 0 until a loop exists

    // Loop position, or time since the take started while the first take records.
    [[nodiscard]] std::int64_t position() const noexcept;

    [[nodiscard]] Result lastResult() const noexcept;
    [[nodiscard]] std::uint32_t resultCount() const noexcept;

  private:
    std::atomic<Transport> transport_{Transport::Empty};
    std::array<std::atomic<TrackState>, kTrackCount> tracks_{};
    std::atomic<std::int64_t> now_{0};
    std::atomic<std::int64_t> loopLength_{0};
    std::atomic<std::int64_t> position_{0};
    std::atomic<Result> lastResult_{Result::Accepted};
    std::atomic<std::uint32_t> resultCount_{0};

    static_assert(std::atomic<std::int64_t>::is_always_lock_free,
                  "the audio thread must never lock to publish");
};

} // namespace als
