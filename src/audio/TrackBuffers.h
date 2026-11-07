#pragma once

#include "core/LoopController.h"

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace als {

// Track buffers will provide memory for storage of 4 tracks. Timing & play to be managed by LoopController.
// Each buffer to hold one mono track.
class TrackBuffers {
  public:
    void prepare(int capacityInSamples);   // Allocate enough memory to hold requested num of samles.
    [[nodiscard]] int capacity() const noexcept;
    void clearTrack(std::size_t track) noexcept;   // Clear audio of track, retain buffer
    [[nodiscard]] int recordedLength(std::size_t track) const noexcept;
    [[nodiscard]] int appendSamples(std::size_t track, const float* inputSamples, int sampleCount) noexcept;   // Adds microphone samples to track
    [[nodiscard]] float sampleAt(std::size_t track, int position) const noexcept;   // Reads one sample
    [[nodiscard]] const float* data(std::size_t track) const noexcept;

  private:
    std::array<juce::AudioBuffer<float>, kTrackCount> buffers;   // Array containing the four audio buffers
    int capacityInSamples_ = 0;
    std::array<int, kTrackCount> recordedLengths{};
};

} // namespace als