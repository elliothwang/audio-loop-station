#pragma once

#include "LoopTiming.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace als {

constexpr std::size_t kTrackCount = 4;

enum class TrackState { Empty, Recording, Playing };
enum class Transport { Empty, RecordingFirstTake, Playing, Stopped };
enum class Command { Record, Play, Stop, DeleteTrack };
enum class Result { Accepted, Ignored, TracksFull, TakeTooShort, InvalidTrack };

// The looper's state machine. Owned by the audio thread; commands arrive between blocks.
class LoopController {
  public:
    // Take lengths are in samples. A first take is discarded below the minimum and ends at the
    // maximum, which is the track buffer's capacity.
    LoopController(std::int64_t minimumTakeLength, std::int64_t maximumTakeLength) noexcept;

    // `track` is only read by DeleteTrack. It has no default so a delete always names its track.
    [[nodiscard]] Result handle(Command command, std::int64_t now, std::size_t track) noexcept;

    // Call once per block. Ends a take that has reached its full length.
    void advance(std::int64_t now) noexcept;

    [[nodiscard]] Transport transport() const noexcept;
    [[nodiscard]] TrackState track(std::size_t index) const noexcept;
    [[nodiscard]] const std::optional<LoopTiming>& loop() const noexcept;
    [[nodiscard]] std::optional<std::size_t> recordingTrack() const noexcept;
    [[nodiscard]] std::optional<std::int64_t> takeStart() const noexcept;

    // Where input stops being written: one loop after the take started, or the buffer's capacity
    // for the first take.
    [[nodiscard]] std::optional<std::int64_t> takeEnd() const noexcept;

    [[nodiscard]] bool tracksFull() const noexcept;

    // True if the track should sound at `time`. A take sounds from its end even before advance()
    // runs, so a take that ends mid-block repeats without a gap.
    [[nodiscard]] bool audible(std::size_t index, std::int64_t time) const noexcept;

  private:
    Result record(std::int64_t now) noexcept;
    Result finishFirstTake(std::int64_t stop, bool play) noexcept;
    Result deleteTrack(std::size_t index) noexcept;
    std::optional<std::size_t> firstEmptyTrack() const noexcept;

    std::int64_t minimumTakeLength_;
    std::int64_t maximumTakeLength_;
    bool playing_ = false;
    std::array<TrackState, kTrackCount> tracks_{};
    std::optional<LoopTiming> loop_;
    std::optional<std::int64_t> takeStart_;
};

} // namespace als
