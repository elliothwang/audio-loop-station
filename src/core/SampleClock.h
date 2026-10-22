#pragma once

// Shared timing base for playback, recording, and looping. Counts the samples the audio callback
// has processed since the stream started, so it runs on the audio device's own clock and every
// track reads the same time. It only ever counts up; a loop is a reading of this count (see
// LoopTiming.h). Owned and written by the audio thread.

#include <cstdint>

namespace als {

class SampleClock {
  public:
    // Returns the samples processed since the stream started.
    [[nodiscard]] std::int64_t now() const noexcept;

    // Moves the clock forward by one audio block. Call once per callback, after the block.
    // sampleCount: the block's actual sample count, which may differ between callbacks. Must not
    // be negative; a negative count asserts in debug builds and is ignored in release builds.
    void advance(std::int64_t sampleCount) noexcept;

  private:
    std::int64_t now_ = 0;
};

} // namespace als
