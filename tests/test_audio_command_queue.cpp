#include "AudioCommandQueue.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

TEST_CASE("command queue starts empty", "[command_queue]") {
    als::AudioCommandQueue queue;
    als::AudioCommand received;

    REQUIRE_FALSE(queue.pop(received));
}

TEST_CASE("command queue preserves command actions and order", "[command_queue]") {
    als::AudioCommandQueue queue;

    REQUIRE(queue.push({als::Command::Record, 0}));
    REQUIRE(queue.push({als::Command::Stop, 0}));
    REQUIRE(queue.push({als::Command::DeleteTrack, 2}));

    als::AudioCommand received;

    REQUIRE(queue.pop(received));
    REQUIRE(received.command == als::Command::Record);

    REQUIRE(queue.pop(received));
    REQUIRE(received.command == als::Command::Stop);

    REQUIRE(queue.pop(received));
    REQUIRE(received.command == als::Command::DeleteTrack);
    REQUIRE(received.track == 2);

    REQUIRE_FALSE(queue.pop(received));
}

TEST_CASE("full queue refuses new additions but maintains all previous added commands",
          "[command_queue]") {
    als::AudioCommandQueue queue;

    for (int i = 0; i < 64; ++i) {
        REQUIRE(queue.push({als::Command::Record, 0}));
    }

    REQUIRE_FALSE(queue.push({als::Command::Stop, 0}));

    als::AudioCommand received;

    for (int i = 0; i < 64; ++i) {
        REQUIRE(queue.pop(received));
        REQUIRE(received.command == als::Command::Record);
    }

    REQUIRE_FALSE(queue.pop(received));
}

TEST_CASE("queue slots reused with wrap-around", "[command_queue]") {
    als::AudioCommandQueue queue;
    als::AudioCommand received;

    // Repeated batches move the read and write positions past the array's end.
    for (int batch = 0; batch < 10; ++batch) {
        for (std::size_t i = 0; i < 32; ++i) {
            REQUIRE(queue.push({als::Command::DeleteTrack, i % als::kTrackCount}));
        }

        for (std::size_t i = 0; i < 32; ++i) {
            REQUIRE(queue.pop(received));
            REQUIRE(received.command == als::Command::DeleteTrack);
            REQUIRE(received.track == i % als::kTrackCount);
        }

        REQUIRE_FALSE(queue.pop(received));
    }
}