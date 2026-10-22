#pragma once

#include <cstdint>

namespace als {

// Samples processed by the audio callback since the stream started. Only counts up.
class SampleClock {
  public:
    [[nodiscard]] std::int64_t now() const noexcept;

    // Call once per callback with the block's sample count.
    void advance(std::int64_t sampleCount) noexcept;

  private:
    std::int64_t now_ = 0;
};

} // namespace als
