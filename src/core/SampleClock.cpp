#include "SampleClock.h"

#include <cassert>

namespace als {

std::int64_t SampleClock::now() const noexcept {
    return now_;
}

void SampleClock::advance(std::int64_t sampleCount) noexcept {
    assert(sampleCount >= 0 && "sampleCount must not be negative");
    // Release builds drop the assert; ignoring a negative count keeps the clock monotonic.
    if (sampleCount < 0) {
        return;
    }
    now_ += sampleCount;
}

} // namespace als
