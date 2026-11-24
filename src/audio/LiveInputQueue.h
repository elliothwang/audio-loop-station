#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <vector>

namespace als {

// Samples in one chunk, matching an AudioThumbnail point so a chunk never spans two.
constexpr int kLiveInputChunk = 256;

// Part of a take as it records, with everything needed to draw it in the take's lane.
struct LiveInput {
    std::size_t track = 0;
    std::int64_t take = 0;       // [samples] clock time the take started, unique to each take
    std::int64_t position = 0;   // [samples] loop position of samples[0], or time into a first take
    std::int64_t loopLength = 0; // [samples] 0 while the first take records
    int count = 0;
    std::array<float, kLiveInputChunk> samples{};
};

// Audio thread is chunk creator, UI thread is chunk consumer.
class LiveInputQueue {
  public:
    // Chunks the queue holds, about 2.7 s at 48 kHz, so a stalled UI only loses input after that.
    static constexpr int kCapacity = 512;

    // Audio thread adds a chunk. Returns false and drops the chunk if the queue is full.
    [[nodiscard]] bool push(const LiveInput& chunk) noexcept {
        const auto slot = fifo.write(1);

        if (slot.blockSize1 == 0) {
            return false;
        }

        chunks[static_cast<std::size_t>(slot.startIndex1)] = chunk;
        return true;
    }

    // UI thread takes the oldest chunk. Returns false if the queue is empty.
    [[nodiscard]] bool pop(LiveInput& chunk) noexcept {
        const auto slot = fifo.read(1);

        if (slot.blockSize1 == 0) {
            return false;
        }

        chunk = chunks[static_cast<std::size_t>(slot.startIndex1)];
        return true;
    }

  private:
    // One slot more than the capacity, since the FIFO always leaves one empty.
    static constexpr int storageSize = kCapacity + 1;

    // Allocated once here, about 530 KB, so the audio thread never allocates.
    std::vector<LiveInput> chunks = std::vector<LiveInput>(storageSize);
    juce::AbstractFifo fifo{storageSize};
};

} // namespace als
