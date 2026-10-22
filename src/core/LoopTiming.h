#pragma once

// Loop timing on top of SampleClock. A loop is not a clock of its own: it is an origin (the
// downbeat, set by the first RECORD press) and a length, and a loop position is a reading of
// SampleClock time. Track read and write indexes are pure mappings of a loop position that apply
// reverse, slip, and latency compensation. Every function is constant time and never allocates,
// locks, or throws, so the audio callback may call them.

#include <cstddef>
#include <cstdint>
#include <optional>

namespace als {

// Where a loop sits on the SampleClock timeline.
struct LoopTiming {
    std::int64_t origin = 0; // SampleClock time of the downbeat [samples]
    std::int64_t length = 0; // samples per pass; positive once a loop exists [samples]
};

// Defines the loop from the first take.
// start: SampleClock time of the first RECORD press; becomes the downbeat.
// stop: SampleClock time of the second RECORD press.
// minimumLength: shortest take to keep [samples]; guards against an accidental double press.
// Returns the loop, or std::nullopt if the take is empty, backward, or shorter than minimumLength.
[[nodiscard]] std::optional<LoopTiming> loopFromFirstTake(std::int64_t start, std::int64_t stop,
                                                          std::int64_t minimumLength) noexcept;

// Maps a SampleClock time to a position in the loop.
// loop: the loop; its length must be positive.
// time: any SampleClock time, including times before the origin.
// Returns a position in [0, loop.length).
[[nodiscard]] std::int64_t loopPosition(const LoopTiming& loop, std::int64_t time) noexcept;

// Maps a loop position to the index of the track sample to play.
// position: position in the loop; any value, so callers may pass a block's start position + i.
// length: loop length [samples]; must be positive.
// slip: offset [samples]; positive makes the track sound later and negative earlier, whether or
// not the track is reversed.
// reversed: true to play the track from its end toward its start.
// Returns an index in [0, length).
[[nodiscard]] std::size_t trackReadIndex(std::int64_t position, std::int64_t length,
                                         std::int64_t slip, bool reversed) noexcept;

// Maps a loop position to the index where the input sample arriving now should be recorded.
// Input arrives late by the round-trip latency, so it belongs that many samples earlier.
// position: position in the loop; any value, so callers may pass a block's start position + i.
// length: loop length [samples]; must be positive.
// latency: round-trip input plus output latency [samples]; must not be negative.
// Returns an index in [0, length).
[[nodiscard]] std::size_t trackWriteIndex(std::int64_t position, std::int64_t length,
                                          std::int64_t latency) noexcept;

} // namespace als
