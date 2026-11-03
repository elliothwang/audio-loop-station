#include "core/LoopController.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

using als::Command;
using als::LoopController;
using als::Result;
using als::TrackState;
using als::Transport;

namespace {

constexpr std::int64_t kMinimumTake = 10;
constexpr std::int64_t kMaximumTake = 1000;

LoopController makeController() {
    return LoopController(kMinimumTake, kMaximumTake);
}

// A first take from 100 to 300, so the loop is 200 samples and playing.
LoopController makePlayingLoop() {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Record, 300, 0) == Result::Accepted);
    return controller;
}

} // namespace

TEST_CASE("a new controller is empty", "[loop_controller]") {
    const LoopController controller = makeController();
    REQUIRE(controller.transport() == Transport::Empty);
    REQUIRE_FALSE(controller.loop());
    REQUIRE_FALSE(controller.recordingTrack());
    for (std::size_t i = 0; i < als::kTrackCount; ++i) {
        REQUIRE(controller.track(i) == TrackState::Empty);
    }
}

TEST_CASE("record from empty starts the first take", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::RecordingFirstTake);
    REQUIRE(controller.recordingTrack() == 0u);
    REQUIRE(controller.takeStart() == 100);
    REQUIRE(controller.takeEnd() == 100 + kMaximumTake);
}

TEST_CASE("record again ends the first take and plays it", "[loop_controller]") {
    const LoopController controller = makePlayingLoop();
    REQUIRE(controller.transport() == Transport::Playing);
    REQUIRE(controller.loop()->origin == 100);
    REQUIRE(controller.loop()->length == 200);
    REQUIRE(controller.track(0) == TrackState::Playing);
    REQUIRE_FALSE(controller.takeStart());
}

TEST_CASE("a first take under the minimum is discarded", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Record, 100 + kMinimumTake - 1, 0) == Result::TakeTooShort);
    REQUIRE(controller.transport() == Transport::Empty);
    REQUIRE(controller.track(0) == TrackState::Empty);
}

TEST_CASE("the first take ends and plays at the buffer's capacity", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    controller.advance(100 + kMaximumTake - 1);
    REQUIRE(controller.transport() == Transport::RecordingFirstTake);
    controller.advance(100 + kMaximumTake + 7);
    REQUIRE(controller.transport() == Transport::Playing);
    REQUIRE(controller.loop()->length == kMaximumTake);
}

TEST_CASE("stop during the first take keeps it, stopped", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Stop, 300, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::Stopped);
    REQUIRE(controller.loop()->length == 200);
    REQUIRE(controller.track(0) == TrackState::Playing);
}

TEST_CASE("record while playing takes the next empty track", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    REQUIRE(controller.recordingTrack() == 1u);
    REQUIRE(controller.takeEnd() == 600);
    REQUIRE(controller.transport() == Transport::Playing);
}

TEST_CASE("record again ends a later take", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Record, 450, 0) == Result::Accepted);
    REQUIRE(controller.track(1) == TrackState::Playing);
    REQUIRE_FALSE(controller.recordingTrack());
}

TEST_CASE("a later take ends after exactly one loop", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    controller.advance(599);
    REQUIRE(controller.track(1) == TrackState::Recording);
    controller.advance(600);
    REQUIRE(controller.track(1) == TrackState::Playing);
}

TEST_CASE("record with every track full is refused", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    for (std::int64_t t = 400; t < 700; t += 100) {
        REQUIRE(controller.handle(Command::Record, t, 0) == Result::Accepted);
        REQUIRE(controller.handle(Command::Record, t + 50, 0) == Result::Accepted);
    }
    REQUIRE(controller.tracksFull());
    REQUIRE(controller.handle(Command::Record, 800, 0) == Result::TracksFull);
    REQUIRE_FALSE(controller.recordingTrack());
}

TEST_CASE("stop while recording keeps the partial take", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Stop, 450, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::Stopped);
    REQUIRE(controller.track(1) == TrackState::Playing);
}

TEST_CASE("play restarts the loop from the top", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Stop, 350, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Play, 1000, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::Playing);
    REQUIRE(controller.loop()->origin == 1000);
}

TEST_CASE("record while stopped restarts and records", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Stop, 350, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Record, 1000, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::Playing);
    REQUIRE(controller.loop()->origin == 1000);
    REQUIRE(controller.recordingTrack() == 1u);
}

TEST_CASE("play and stop are ignored when they don't apply", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Play, 0, 0) == Result::Ignored);
    REQUIRE(controller.handle(Command::Stop, 0, 0) == Result::Ignored);

    controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Play, 350, 0) == Result::Ignored);
    REQUIRE(controller.handle(Command::Stop, 350, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Stop, 360, 0) == Result::Ignored);
}

TEST_CASE("deleting a track empties it and keeps the loop", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Record, 450, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::DeleteTrack, 500, 1) == Result::Accepted);
    REQUIRE(controller.track(1) == TrackState::Empty);
    REQUIRE(controller.transport() == Transport::Playing);
}

TEST_CASE("deleting the last track clears the loop", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::DeleteTrack, 400, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::Empty);
    REQUIRE_FALSE(controller.loop());
    REQUIRE(controller.handle(Command::Record, 500, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::RecordingFirstTake);
}

TEST_CASE("deleting the recording track cancels its take", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::DeleteTrack, 450, 1) == Result::Accepted);
    REQUIRE(controller.track(1) == TrackState::Empty);
    REQUIRE_FALSE(controller.takeStart());
    REQUIRE(controller.transport() == Transport::Playing);

    controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::DeleteTrack, 150, 0) == Result::Accepted);
    REQUIRE(controller.transport() == Transport::Empty);
}

TEST_CASE("deleting an empty or missing track does nothing", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::DeleteTrack, 400, 2) == Result::Ignored);
    REQUIRE(controller.handle(Command::DeleteTrack, 400, als::kTrackCount) == Result::InvalidTrack);
    REQUIRE(controller.track(0) == TrackState::Playing);
}

TEST_CASE("record while stopped with every track full changes nothing", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    for (std::int64_t t = 400; t < 700; t += 100) {
        REQUIRE(controller.handle(Command::Record, t, 0) == Result::Accepted);
        REQUIRE(controller.handle(Command::Record, t + 50, 0) == Result::Accepted);
    }
    REQUIRE(controller.handle(Command::Stop, 750, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Record, 2000, 0) == Result::TracksFull);
    REQUIRE(controller.transport() == Transport::Stopped);
    REQUIRE(controller.loop()->origin == 100);
}

TEST_CASE("stop during a too-short first take discards it", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE(controller.handle(Command::Stop, 105, 0) == Result::TakeTooShort);
    REQUIRE(controller.transport() == Transport::Empty);
    REQUIRE(controller.track(0) == TrackState::Empty);
}

TEST_CASE("advance with no take in progress changes nothing", "[loop_controller]") {
    LoopController controller = makeController();
    controller.advance(5000);
    REQUIRE(controller.transport() == Transport::Empty);

    controller = makePlayingLoop();
    controller.advance(5000);
    REQUIRE(controller.transport() == Transport::Playing);
    REQUIRE(controller.track(0) == TrackState::Playing);
    REQUIRE(controller.track(1) == TrackState::Empty);
}

TEST_CASE("a take ending mid-block is audible from its end", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Record, 400, 0) == Result::Accepted);
    REQUIRE(controller.audible(0, 500));
    REQUIRE_FALSE(controller.audible(1, 599));
    REQUIRE(controller.audible(1, 600)); // before advance() has ended the take
    REQUIRE_FALSE(controller.audible(2, 600));
}

TEST_CASE("a first take is audible from the moment it fills the buffer", "[loop_controller]") {
    LoopController controller = makeController();
    REQUIRE(controller.handle(Command::Record, 100, 0) == Result::Accepted);
    REQUIRE_FALSE(controller.audible(0, 100 + kMaximumTake - 1));
    REQUIRE(controller.audible(0, 100 + kMaximumTake));
}

TEST_CASE("nothing is audible while stopped", "[loop_controller]") {
    LoopController controller = makePlayingLoop();
    REQUIRE(controller.handle(Command::Stop, 350, 0) == Result::Accepted);
    REQUIRE_FALSE(controller.audible(0, 400));
}

