#pragma once

// Plays the recorded clip on repeat. The message thread starts and stops it, and the audio
// callback reads the clip into the output each block.

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace als {

// Plays from the top on every start. Stopping returns to the top rather than pausing.
class LoopPlayer {
  public:
    // [ms] at each end of the loop, so the wrap doesn't click
    static constexpr int kFadeMilliseconds = 5;

    // Sets the fade length for `sampleRate`. Called while the callback is stopped.
    void prepare(double sampleRate) noexcept;

    // Starts playback from the top at the next block. Message thread.
    void play() noexcept;

    // Stops playback at the next block. Message thread.
    void stop() noexcept;

    // Returns true while playback is requested.
    [[nodiscard]] bool playing() const noexcept;

    // Fills `left` and `right` with `count` samples of the `length` sample `loop`, or does nothing
    // when stopped or there's no loop. `right` may be null for mono output. Audio thread.
    void process(const float* loop, std::size_t length, float* left, float* right,
                 std::size_t count) noexcept;

    // Returns the playback position in samples, or 0 when stopped.
    [[nodiscard]] std::int64_t position() const noexcept;

    // Returns the loop's length in samples as of the last block, or 0 when there's no loop.
    [[nodiscard]] std::int64_t length() const noexcept;

  private:
    std::atomic<bool> playing_{false};
    std::atomic<bool> restart_{false};
    std::atomic<std::int64_t> publishedPosition_{0};
    std::atomic<std::int64_t> publishedLength_{0};
    std::size_t fadeLength_ = 0; // [samples]
    std::size_t position_ = 0;   // audio thread only
};

} // namespace als
