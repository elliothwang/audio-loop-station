#include "TrackBuffers.h"
#include <algorithm>

namespace als {

void TrackBuffers::prepare(int capacityInSamples) {
    // Allocate a buffer for each track, initialized with silence.
    const int requestedCapacity = capacityInSamples > 0 ? capacityInSamples : 0;

    // No change to buffers if same capacity requested.
    if (requestedCapacity == capacityInSamples_) {
        return;
    }

    for (auto& buffer : buffers) {
        buffer.setSize(requestedCapacity > 0 ? 1 : 0, requestedCapacity);
        buffer.clear();   // Fill buffer with 0s, silence
    }

    recordedLengths.fill(0);   // Reset recorded lengths of buffer to 0, silence
    capacityInSamples_ = requestedCapacity;
}

int TrackBuffers::capacity() const noexcept {
    // Return number samples each track buffer able to hold
    return capacityInSamples_;
}

void TrackBuffers::clearTrack(std::size_t track) noexcept {
    // Mark recording as empty
    if (track >= kTrackCount) {
        return;
    }

    recordedLengths[track] = 0;
}

int TrackBuffers::recordedLength(std::size_t track) const noexcept {
    // Returns number of audio samples recorded into track
    return track < kTrackCount ? recordedLengths[track] : 0;
}

int TrackBuffers::appendSamples(std::size_t track, const float* inputSamples, int sampleCount) noexcept {
    // Appends incoming blocks of audio samples to track's buffer. Called repeatedly during recording.
    if (track >= kTrackCount || inputSamples == nullptr || sampleCount <= 0) {
        return 0;
    }

    const int recorded = recordedLengths[track];
    const int remaining = capacityInSamples_ - recorded;
    const int samplesToCopy = std::min(sampleCount, remaining);   // Copy only samples that fit

    if (samplesToCopy <= 0) {
        return 0;
    }

    buffers[track].copyFrom(
        0, recorded, inputSamples, samplesToCopy);

    recordedLengths[track] += samplesToCopy;

    return samplesToCopy;
}

float TrackBuffers::sampleAt(std::size_t track, int position) const noexcept {
    if (track >= kTrackCount || position < 0) {
        return 0.0f;
    }

    // Returns silence if past recorded audio samples
    if (position >= recordedLengths[track]) {
        return 0.0f;
    }

    return buffers[track].getSample(0, position);
}

const float* TrackBuffers::data(std::size_t track) const noexcept {
    if (track >= kTrackCount || recordedLengths[track] == 0) {
        return nullptr;
    }

    return buffers[track].getReadPointer(0);
}

} // namespace als