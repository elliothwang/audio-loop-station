#pragma once

// The looper's state machine, the one owner of transport and track state. The audio thread
// applies commands to it between blocks, and ControllerSnapshot.h publishes its state to the UI.

#include "LoopTiming.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace als {

constexpr std::size_t kTrackCount = 4;

// Whether a track has audio and what it's doing with it. Muted is a setting, not a state.
enum class TrackState { Empty, Recording, Playing };

// The looper as a whole, derived from the stored state rather than kept separately.
enum class Transport { Empty, RecordingFirstTake, Playing, Stopped };

// What the user pressed, queued by the UI and applied by the audio thread.
enum class Command { Record, Play, Stop, DeleteTrack };

// What a command did, so the UI can show a notice when one is refused.
enum class Result { Accepted, Ignored, TracksFull, TakeTooShort, InvalidTrack };

// Owned by the audio thread. Every method is constant time and never allocates, locks, or throws.
class LoopController {
  public:
    // Take lengths are in samples. A first take is discarded below the minimum and ends at the
    // maximum, which is the track buffer's capacity.
    LoopController(std::int64_t minimumTakeLength, std::int64_t maximumTakeLength) noexcept;

    // Applies one command at clock time `now` and returns what it did. `track` is only read by
    // DeleteTrack. It has no default so a delete always names its track.
    [[nodiscard]] Result handle(Command command, std::int64_t now, std::size_t track) noexcept;

    // Replace with previously saved, loaded clip to track 1 at beginning.
    [[nodiscard]] bool loadSingleTrack(std::int64_t now, std::int64_t length) noexcept;

    // Call once per block. Ends a take that has reached its full length.
    void advance(std::int64_t now) noexcept;

    // Returns the looper's overall state.
    [[nodiscard]] Transport transport() const noexcept;

    // Returns one track's state. An out-of-range index asserts and reads as Empty.
    [[nodiscard]] TrackState track(std::size_t index) const noexcept;

    // Returns the loop's timing, or nullopt until the first take finishes.
    [[nodiscard]] const std::optional<LoopTiming>& loop() const noexcept;

    // Returns the track being recorded, if any.
    [[nodiscard]] std::optional<std::size_t> recordingTrack() const noexcept;

    // Returns the clock time the current take started, if one is recording.
    [[nodiscard]] std::optional<std::int64_t> takeStart() const noexcept;

    // Returns where input stops being written, one loop after the take started or the buffer's
    // capacity for the first take. Nullopt when nothing is recording.
    [[nodiscard]] std::optional<std::int64_t> takeEnd() const noexcept;

    // Returns true when no track is empty, so RECORD would be refused.
    [[nodiscard]] bool tracksFull() const noexcept;

    // True if the track should sound at `time`. A take sounds from its end even before advance()
    // runs, so a take that ends mid-block repeats without a gap.
    [[nodiscard]] bool audible(std::size_t index, std::int64_t time) const noexcept;

  private:
    // RECORD's transitions, which depend on the transport.
    Result record(std::int64_t now) noexcept;

    // Turns the first take into the loop, or discards it if it's too short.
    Result finishFirstTake(std::int64_t stop, bool play) noexcept;

    // Empties a track, cancelling its take, and clears the loop when no track is left.
    Result deleteTrack(std::size_t index) noexcept;

    // Returns the lowest-numbered empty track, if any.
    std::optional<std::size_t> firstEmptyTrack() const noexcept;

    std::int64_t minimumTakeLength_;
    std::int64_t maximumTakeLength_;
    bool playing_ = false;
    std::array<TrackState, kTrackCount> tracks_{};
    std::optional<LoopTiming> loop_;
    std::optional<std::int64_t> takeStart_;
};

} // namespace als
