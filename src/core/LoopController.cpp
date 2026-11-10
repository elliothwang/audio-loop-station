#include "LoopController.h"

#include <algorithm>
#include <cassert>

namespace als {

LoopController::LoopController(std::int64_t minimumTakeLength,
                               std::int64_t maximumTakeLength) noexcept
    : minimumTakeLength_(minimumTakeLength), maximumTakeLength_(maximumTakeLength) {
    assert(minimumTakeLength > 0 && maximumTakeLength >= minimumTakeLength &&
           "take lengths must be positive and ordered");
}

Result LoopController::handle(Command command, std::int64_t now, std::size_t track) noexcept {
    switch (command) {
    case Command::Record:
        return record(now);
    case Command::Play:
        if (transport() != Transport::Stopped) {
            return Result::Ignored;
        }
        loop_->origin = now; // a loop always restarts from the top
        playing_ = true;
        return Result::Accepted;
    case Command::Stop:
        // STOP during the first take keeps it, so a performer who stops by habit still has a loop.
        if (transport() == Transport::RecordingFirstTake) {
            return finishFirstTake(now, false);
        }
        if (transport() != Transport::Playing) {
            return Result::Ignored;
        }
        if (const auto recording = recordingTrack()) {
            tracks_[*recording] = TrackState::Playing; // keep what was recorded so far
            takeStart_.reset();
        }
        playing_ = false;
        return Result::Accepted;
    case Command::DeleteTrack:
        return deleteTrack(track);
    }
    return Result::Ignored;
}

void LoopController::advance(std::int64_t now) noexcept {
    // `now` is the end of a block and can overshoot the take's end, so the take ends at `end`,
    // which keeps the first take's length exactly the buffer's capacity.
    const std::optional<std::int64_t> end = takeEnd();
    if (!end || now < *end) {
        return;
    }
    if (transport() == Transport::RecordingFirstTake) {
        finishFirstTake(*end, true); // a full buffer is always long enough
        return;
    }
    assert(recordingTrack() && "take in progress without a recording track");
    tracks_[*recordingTrack()] = TrackState::Playing;
    takeStart_.reset();
}

Result LoopController::record(std::int64_t now) noexcept {
    // Playing and Stopped both fall through to starting a later take, which differs only in
    // whether playback has to restart first.
    switch (transport()) {
    case Transport::Empty:
        tracks_[0] = TrackState::Recording;
        takeStart_ = now;
        return Result::Accepted;
    case Transport::RecordingFirstTake:
        return finishFirstTake(now, true);
    case Transport::Playing:
        if (const auto recording = recordingTrack()) {
            tracks_[*recording] = TrackState::Playing;
            takeStart_.reset();
            return Result::Accepted;
        }
        break;
    case Transport::Stopped:
        break;
    }

    // Start a later take. RECORD while stopped also restarts playback from the top.
    const std::optional<std::size_t> free = firstEmptyTrack();
    if (!free) {
        return Result::TracksFull;
    }
    if (!playing_) {
        loop_->origin = now;
        playing_ = true;
    }
    tracks_[*free] = TrackState::Recording;
    takeStart_ = now;
    return Result::Accepted;
}

Result LoopController::finishFirstTake(std::int64_t stop, bool play) noexcept {
    // Clear the take first so both outcomes leave nothing recording.
    const std::optional<LoopTiming> loop = loopFromFirstTake(*takeStart_, stop, minimumTakeLength_);
    takeStart_.reset();
    if (!loop) {
        // The first take is always track 0, since every track was empty when it started.
        tracks_[0] = TrackState::Empty;
        return Result::TakeTooShort;
    }
    loop_ = loop;
    tracks_[0] = TrackState::Playing;
    // False when STOP ended the take, so the loop exists but waits for PLAY.
    playing_ = play;
    return Result::Accepted;
}

Result LoopController::deleteTrack(std::size_t index) noexcept {
    if (index >= kTrackCount) {
        return Result::InvalidTrack;
    }
    if (tracks_[index] == TrackState::Empty) {
        return Result::Ignored;
    }
    // A take is only its start time here, so clearing it cancels the take. The engine stops
    // writing input because the track is no longer Recording.
    if (tracks_[index] == TrackState::Recording) {
        takeStart_.reset();
    }
    tracks_[index] = TrackState::Empty;
    const bool anyLeft = std::any_of(tracks_.begin(), tracks_.end(),
                                     [](TrackState state) { return state != TrackState::Empty; });
    if (anyLeft) {
        return Result::Accepted;
    }
    // The last track is gone, so the next RECORD sets a new downbeat.
    loop_.reset();
    playing_ = false;
    return Result::Accepted;
}

Transport LoopController::transport() const noexcept {
    if (loop_) {
        return playing_ ? Transport::Playing : Transport::Stopped;
    }
    return takeStart_ ? Transport::RecordingFirstTake : Transport::Empty;
}

TrackState LoopController::track(std::size_t index) const noexcept {
    assert(index < kTrackCount && "track index out of range");
    return index < kTrackCount ? tracks_[index] : TrackState::Empty;
}

const std::optional<LoopTiming>& LoopController::loop() const noexcept {
    return loop_;
}

std::optional<std::size_t> LoopController::recordingTrack() const noexcept {
    for (std::size_t i = 0; i < kTrackCount; ++i) {
        if (tracks_[i] == TrackState::Recording) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::int64_t> LoopController::takeStart() const noexcept {
    return takeStart_;
}

std::optional<std::int64_t> LoopController::takeEnd() const noexcept {
    if (!takeStart_) {
        return std::nullopt;
    }
    // The first take has no loop length yet, so its limit is the buffer's capacity.
    return *takeStart_ + (loop_ ? loop_->length : maximumTakeLength_);
}

bool LoopController::tracksFull() const noexcept {
    return !firstEmptyTrack();
}

bool LoopController::audible(std::size_t index, std::int64_t time) const noexcept {
    const TrackState state = track(index);
    if (state == TrackState::Playing) {
        return transport() == Transport::Playing;
    }
    // advance() only runs after a block, so a take that ends mid-block is still Recording.
    // Treating it as audible from its end lets it repeat without a gap.
    const std::optional<std::int64_t> end = takeEnd();
    return state == TrackState::Recording && end && time >= *end;
}

std::optional<std::size_t> LoopController::firstEmptyTrack() const noexcept {
    for (std::size_t i = 0; i < kTrackCount; ++i) {
        if (tracks_[i] == TrackState::Empty) {
            return i;
        }
    }
    return std::nullopt;
}

bool LoopController::loadSingleTrack(std::int64_t now, std::int64_t length) noexcept {
    // Loads previously saved single track to track 1. Will likely need to update/remove once full
    // loop saving enabled.S
    if (length <= 0 || length > maximumTakeLength_) {
        return false;
    }

    tracks_.fill(TrackState::Empty);
    tracks_[0] = TrackState::Playing;
    loop_ = LoopTiming{now, length};
    takeStart_.reset();
    playing_ = false;
    return true;
}

} // namespace als
