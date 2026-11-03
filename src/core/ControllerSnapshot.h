#pragma once

// The UI's view of the LoopController. The audio thread publishes into it once per block and the
// UI thread reads it, so the UI never touches the controller itself.

#include "LoopController.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace als {

// Lock-free copy of the controller's state. Fields are read independently, so one read can mix
// values from blocks next to each other, which is fine for display.
class ControllerSnapshot {
  public:
    // Copies the controller's state as of clock time `now`. Audio thread only.
    void publish(const LoopController& controller, std::int64_t now) noexcept;

    // Records a command's result and bumps resultCount, so the UI can show a repeated notice once
    // per press. Audio thread only.
    void publishResult(Result result) noexcept;

    // Returns the published transport.
    [[nodiscard]] Transport transport() const noexcept;

    // Returns one track's published state. An out-of-range index asserts and reads as Empty.
    [[nodiscard]] TrackState track(std::size_t index) const noexcept;

    // Returns the clock time of the last publish.
    [[nodiscard]] std::int64_t now() const noexcept;

    // Returns the loop length in samples, or 0 until a loop exists.
    [[nodiscard]] std::int64_t loopLength() const noexcept;

    // Returns the loop position, or the time since the take started while the first take records.
    [[nodiscard]] std::int64_t position() const noexcept;

    // Returns the most recent command result.
    [[nodiscard]] Result lastResult() const noexcept;

    // Returns how many results have been published. A change means a new result to show.
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
