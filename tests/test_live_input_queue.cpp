#include "LiveInputQueue.h"

#include <catch2/catch_test_macros.hpp>

namespace {

// Returns a chunk of `count` samples, each set to `value`.
als::LiveInput chunkOf(std::size_t track, std::int64_t position, int count, float value) {
    als::LiveInput chunk;
    chunk.track = track;
    chunk.take = 100;
    chunk.position = position;
    chunk.loopLength = 48000;
    chunk.count = count;
    chunk.samples.fill(value);
    return chunk;
}

} // namespace

TEST_CASE("live input queue starts empty", "[live_input_queue]") {
    als::LiveInputQueue queue;
    als::LiveInput received;

    REQUIRE_FALSE(queue.pop(received));
}

TEST_CASE("live input queue preserves chunks and their order", "[live_input_queue]") {
    als::LiveInputQueue queue;

    REQUIRE(queue.push(chunkOf(1, 0, als::kLiveInputChunk, 0.5f)));
    REQUIRE(queue.push(chunkOf(2, 256, 10, -0.25f)));

    als::LiveInput received;

    REQUIRE(queue.pop(received));
    REQUIRE(received.track == 1);
    REQUIRE(received.take == 100);
    REQUIRE(received.position == 0);
    REQUIRE(received.loopLength == 48000);
    REQUIRE(received.count == als::kLiveInputChunk);
    REQUIRE(received.samples.back() == 0.5f);

    REQUIRE(queue.pop(received));
    REQUIRE(received.track == 2);
    REQUIRE(received.position == 256);
    REQUIRE(received.count == 10);
    REQUIRE(received.samples.front() == -0.25f);

    REQUIRE_FALSE(queue.pop(received));
}

TEST_CASE("full live input queue drops new chunks and keeps the queued ones",
          "[live_input_queue]") {
    als::LiveInputQueue queue;

    for (int i = 0; i < als::LiveInputQueue::kCapacity; ++i) {
        REQUIRE(queue.push(chunkOf(0, i, 1, 0.0f)));
    }

    REQUIRE_FALSE(queue.push(chunkOf(0, als::LiveInputQueue::kCapacity, 1, 0.0f)));

    als::LiveInput received;

    for (int i = 0; i < als::LiveInputQueue::kCapacity; ++i) {
        REQUIRE(queue.pop(received));
        REQUIRE(received.position == i);
    }

    REQUIRE_FALSE(queue.pop(received));
}
