#include "storage/LoopFile.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

using als::SavedLoop;

namespace {

constexpr std::int64_t kLoopLength = 1000;

// A fresh folder under the system temp directory, deleted when the test ends.
struct TemporaryFolder {
    TemporaryFolder()
        : root(juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getNonexistentChildFile("als_test_loops", "")) {
        REQUIRE(root.createDirectory().wasOk());
    }
    ~TemporaryFolder() {
        static_cast<void>(root.deleteRecursively());
    }
    TemporaryFolder(const TemporaryFolder&) = delete;
    TemporaryFolder& operator=(const TemporaryFolder&) = delete;

    juce::File root;
};

// Samples that aren't round numbers, so a lossy format would change them.
std::vector<float> makeSamples(std::size_t count, float offset) {
    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
        samples[i] = static_cast<float>(i) / 997.0f - offset;
    }
    return samples;
}

// Track 1 fills the loop, track 3 is a shorter take that wraps past the end, and tracks 2 and 4
// are empty.
SavedLoop makeLoop() {
    SavedLoop loop;
    loop.sampleRate = 48000.0;
    loop.length = kLoopLength;
    loop.tracks[0].samples = makeSamples(static_cast<std::size_t>(kLoopLength), 0.5f);
    loop.tracks[2].samples = makeSamples(300, 0.25f);
    loop.tracks[2].start = 850;
    return loop;
}

} // namespace

TEST_CASE("a saved loop loads back exactly", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Exact");
    const SavedLoop saved = makeLoop();
    REQUIRE(als::saveLoop(saved, loopFolder).wasOk());
    REQUIRE(loopFolder.getChildFile("loop.json").existsAsFile());
    REQUIRE(loopFolder.getChildFile("track1.wav").existsAsFile());
    REQUIRE(loopFolder.getChildFile("track3.wav").existsAsFile());
    REQUIRE_FALSE(loopFolder.getChildFile("track2.wav").exists()); // empty tracks have no WAV

    SavedLoop loaded;
    REQUIRE(als::loadLoop(loopFolder, loaded).wasOk());
    REQUIRE(loaded.sampleRate == saved.sampleRate);
    REQUIRE(loaded.length == saved.length);
    for (std::size_t i = 0; i < als::kTrackCount; ++i) {
        REQUIRE(loaded.tracks[i].samples == saved.tracks[i].samples);
        REQUIRE(loaded.tracks[i].start == saved.tracks[i].start);
    }
}

TEST_CASE("a loop with only a later track loads back", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Later");
    SavedLoop saved = makeLoop();
    saved.tracks[0].samples.clear(); // track 1 was deleted after the others were recorded
    REQUIRE(als::saveLoop(saved, loopFolder).wasOk());

    SavedLoop loaded;
    REQUIRE(als::loadLoop(loopFolder, loaded).wasOk());
    REQUIRE(loaded.tracks[0].samples.empty());
    REQUIRE(loaded.tracks[2].samples == saved.tracks[2].samples);
}

TEST_CASE("a loop isn't saved over an existing one", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Taken");
    REQUIRE(als::saveLoop(makeLoop(), loopFolder).wasOk());

    SavedLoop other = makeLoop();
    other.tracks[0].samples.assign(10, 0.25f);
    REQUIRE(als::saveLoop(other, loopFolder).failed());

    SavedLoop loaded;
    REQUIRE(als::loadLoop(loopFolder, loaded).wasOk());
    REQUIRE(loaded.tracks[0].samples == makeLoop().tracks[0].samples);
}

TEST_CASE("an empty loop isn't saved", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Empty");

    SavedLoop noAudio = makeLoop();
    for (als::SavedTrack& track : noAudio.tracks) {
        track.samples.clear();
    }
    REQUIRE(als::saveLoop(noAudio, loopFolder).failed());

    SavedLoop noRate = makeLoop();
    noRate.sampleRate = 0.0;
    REQUIRE(als::saveLoop(noRate, loopFolder).failed());

    SavedLoop noLength = makeLoop();
    noLength.length = 0;
    REQUIRE(als::saveLoop(noLength, loopFolder).failed());
    REQUIRE_FALSE(loopFolder.exists());
}

TEST_CASE("a take that doesn't fit the loop isn't saved", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Misfit");

    SavedLoop tooLong = makeLoop();
    tooLong.tracks[1].samples.assign(static_cast<std::size_t>(kLoopLength) + 1, 0.25f);
    REQUIRE(als::saveLoop(tooLong, loopFolder).failed());

    SavedLoop startPastEnd = makeLoop();
    startPastEnd.tracks[2].start = kLoopLength;
    REQUIRE(als::saveLoop(startPastEnd, loopFolder).failed());

    SavedLoop startBeforeTop = makeLoop();
    startBeforeTop.tracks[2].start = -1;
    REQUIRE(als::saveLoop(startBeforeTop, loopFolder).failed());
    REQUIRE_FALSE(loopFolder.exists());
}

TEST_CASE("loading a missing or corrupt loop fails", "[loop_file]") {
    const TemporaryFolder folder;
    SavedLoop loaded = makeLoop();

    REQUIRE(als::loadLoop(folder.root.getChildFile("Missing"), loaded).failed());

    const juce::File corrupt = folder.root.getChildFile("Corrupt");
    REQUIRE(corrupt.createDirectory().wasOk());
    REQUIRE(corrupt.getChildFile("loop.json").replaceWithText("{ not json"));
    REQUIRE(als::loadLoop(corrupt, loaded).failed());

    // loop.json is fine, but a later track's WAV is gone.
    const juce::File noAudio = folder.root.getChildFile("NoAudio");
    REQUIRE(als::saveLoop(makeLoop(), noAudio).wasOk());
    REQUIRE(noAudio.getChildFile("track3.wav").deleteFile());
    REQUIRE(als::loadLoop(noAudio, loaded).failed());

    // A failed load leaves the destination as it was.
    REQUIRE(loaded.tracks[0].samples == makeLoop().tracks[0].samples);
}

TEST_CASE("a loop.json with a bad length or tracks isn't listed or loaded", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File bad = folder.root.getChildFile("Bad");
    REQUIRE(als::saveLoop(makeLoop(), bad).wasOk()); // so track1.wav and track3.wav exist

    const juce::String first = R"({"audio": true, "start": 0})";
    const juce::String third = R"({"audio": true, "start": 850})";
    const juce::String none = R"({"audio": false, "start": 0})";

    // Builds loop.json for a 48 kHz loop, with tracks 2 and 4 empty.
    const auto settings = [&none](const juce::String& length, const juce::String& track1,
                                  const juce::String& track3) {
        return R"({"version": 2, "sampleRate": 48000.0, "length": )" + length + R"(, "tracks": [)" +
               track1 + ", " + none + ", " + track3 + ", " + none + "]}";
    };

    const std::vector<juce::String> cases{
        R"({"version": 2, "sampleRate": 48000.0, "length": 1000})", // no tracks
        R"({"version": 2, "sampleRate": 48000.0, "length": 1000, "tracks": [)" + first + ", " +
            none + ", " + third + "]}",                                // three tracks
        settings("1000", "1", third),                                  // not an object
        settings("1000", R"({"audio": true})", third),                 // no start
        settings("1000", first, R"({"audio": true, "start": 1000})"),  // start past the end
        settings("1000", first, R"({"audio": true, "start": 850.5})"), // fractional start
        settings("1000", first, R"({"audio": true, "start": 1e30})"),  // start too large
        settings("1000.5", first, third),                              // fractional length
        settings("1000", none, none),                                  // no audio
        settings("999", first, third), // track1.wav is longer than the loop
    };
    for (const juce::String& json : cases) {
        INFO(json);
        REQUIRE(bad.getChildFile("loop.json").replaceWithText(json));
        SavedLoop loaded;
        REQUIRE(als::loadLoop(bad, loaded).failed());
    }

    // Every case but the last is caught from loop.json alone, so the list leaves it out too.
    for (std::size_t i = 0; i + 1 < cases.size(); ++i) {
        INFO(cases[i]);
        REQUIRE(bad.getChildFile("loop.json").replaceWithText(cases[i]));
        REQUIRE(als::listLoops(folder.root).empty());
    }
}

TEST_CASE("a loop from the single track format isn't loaded", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File old = folder.root.getChildFile("Old");
    REQUIRE(old.createDirectory().wasOk());
    REQUIRE(old.getChildFile("loop.json")
                .replaceWithText(R"({"version": 1, "sampleRate": 48000.0, "length": 1000})"));

    SavedLoop loaded;
    REQUIRE(als::loadLoop(old, loaded).failed());
    REQUIRE(als::listLoops(folder.root).empty());
}

TEST_CASE("loop names become legal folder names", "[loop_file]") {
    const juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory);
    REQUIRE(als::loopFolder(root, "Bass Line") == root.getChildFile("Bass Line"));
    REQUIRE(als::loopFolder(root, "a/b:c?") == root.getChildFile("abc"));
    REQUIRE(als::loopFolder(root, "  Take 2. ") == root.getChildFile("Take 2"));
    REQUIRE(als::loopFolder(root, "..") == juce::File());
    REQUIRE(als::loopFolder(root, "   ") == juce::File());
}

TEST_CASE("listing loops skips folders that aren't loops", "[loop_file]") {
    const TemporaryFolder folder;
    REQUIRE(als::saveLoop(makeLoop(), folder.root.getChildFile("Loop 10")).wasOk());
    REQUIRE(als::saveLoop(makeLoop(), folder.root.getChildFile("Loop 2")).wasOk());
    REQUIRE(folder.root.getChildFile("Not a loop").createDirectory().wasOk());
    REQUIRE(folder.root.getChildFile("stray.txt").replaceWithText("hello"));

    const std::vector<als::LoopInfo> loops = als::listLoops(folder.root);
    REQUIRE(loops.size() == 2);
    REQUIRE(loops[0].name == "Loop 2"); // natural order, so 2 comes before 10
    REQUIRE(loops[1].name == "Loop 10");
    REQUIRE(loops[0].sampleRate == 48000.0);
    REQUIRE(loops[0].length == kLoopLength);
    REQUIRE(loops[0].folder == folder.root.getChildFile("Loop 2"));
}

TEST_CASE("listing a missing folder finds no loops", "[loop_file]") {
    const TemporaryFolder folder;
    REQUIRE(als::listLoops(folder.root.getChildFile("Missing")).empty());
}
