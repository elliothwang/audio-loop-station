#include "SampleClock.h"

#include <cassert>

namespace als {

std::int64_t SampleClock::now() const noexcept {
    return now_;
}

void SampleClock::advance(std::int64_t sampleCount) noexcept {
    assert(sampleCount >= 0 && "sampleCount must not be negative");
    if (sampleCount > 0) {
        now_ += sampleCount;
    }
}

} // namespace als
