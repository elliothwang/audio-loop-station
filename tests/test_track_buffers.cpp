#include "TrackBuffers.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("prepared track buffers start silent with recordedLength 0", "[track_buffers]") {
    als::TrackBuffers tracks;
    tracks.prepare(8);

    REQUIRE(tracks.capacity() == 8);

    for (std::size_t track = 0; track < als::kTrackCount; ++track) {
        REQUIRE(tracks.recordedLength(track) == 0);
        REQUIRE(tracks.sampleAt(track, 0) == 0.0f);
    }
}

TEST_CASE("incoming blocks append without replacing earlier samples", "[track_buffers]") {
    als::TrackBuffers tracks;
    tracks.prepare(8);

    const float first[]{0.25f, -0.5f}; // first microphone block with 2 samples
    const float second[]{0.75f, 1.0f}; // second microphone block

    REQUIRE(tracks.appendSamples(0, first, 2) == 2);
    REQUIRE(tracks.appendSamples(0, second, 2) == 2);

    REQUIRE(tracks.recordedLength(0) == 4);
    REQUIRE(tracks.sampleAt(0, 0) == 0.25f);
    REQUIRE(tracks.sampleAt(0, 1) == -0.5f);
    REQUIRE(tracks.sampleAt(0, 2) == 0.75f);
    REQUIRE(tracks.sampleAt(0, 3) == 1.0f);
    REQUIRE(tracks.sampleAt(0, 4) == 0.0f);
}

TEST_CASE("track buffers stop writing at capacity", "[track_buffers]") {
    als::TrackBuffers tracks;
    tracks.prepare(3);

    const float input[]{0.25f, 0.5f};

    REQUIRE(tracks.appendSamples(0, input, 2) == 2);
    REQUIRE(tracks.appendSamples(0, input, 2) == 1); // one sample copied
    REQUIRE(tracks.appendSamples(0, input, 2) == 0); // nothing copied

    REQUIRE(tracks.recordedLength(0) == 3);
    REQUIRE(tracks.sampleAt(0, 2) == 0.25f);
    REQUIRE(tracks.sampleAt(0, 3) == 0.0f); // beyond capacity is silent
}

TEST_CASE("clearing one track leaves other tracks unchanged", "[track_buffers]") {
    als::TrackBuffers tracks;
    tracks.prepare(8);

    const float first[]{0.25f, 0.5f};
    const float second[]{-0.5f};

    REQUIRE(tracks.appendSamples(0, first, 2) == 2);
    REQUIRE(tracks.appendSamples(1, second, 1) == 1);

    tracks.clearTrack(0);

    REQUIRE(tracks.recordedLength(0) == 0);
    REQUIRE(tracks.sampleAt(0, 0) == 0.0f); // Track 1 empty
    REQUIRE(tracks.recordedLength(1) == 1);
    REQUIRE(tracks.sampleAt(1, 0) == -0.5f); // Track 2 unaffected by clearing of track 1
}

TEST_CASE("preparing the same capacity preserves recorded audio", "[track_buffers]") {
    als::TrackBuffers tracks;
    tracks.prepare(8);

    const float input[]{0.75f};
    REQUIRE(tracks.appendSamples(2, input, 1) == 1); // one sample recorded into track 3

    tracks.prepare(8);
    REQUIRE(tracks.recordedLength(2) == 1); // If same capacity, recording is preserved
    REQUIRE(tracks.sampleAt(2, 0) == 0.75f);

    tracks.prepare(16);
    REQUIRE(tracks.capacity() == 16);
    REQUIRE(tracks.recordedLength(2) == 0); // Different capacity, recording silent
    REQUIRE(tracks.sampleAt(2, 0) == 0.0f);
}