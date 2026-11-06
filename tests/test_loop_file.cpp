#include "storage/LoopFile.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using als::SavedLoop;

namespace {

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
SavedLoop makeLoop() {
    SavedLoop loop;
    loop.sampleRate = 48000.0;
    loop.samples.resize(1000);
    for (std::size_t i = 0; i < loop.samples.size(); ++i) {
        loop.samples[i] = static_cast<float>(i) / 997.0f - 0.5f;
    }
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

    SavedLoop loaded;
    REQUIRE(als::loadLoop(loopFolder, loaded).wasOk());
    REQUIRE(loaded.sampleRate == saved.sampleRate);
    REQUIRE(loaded.samples == saved.samples);
}

TEST_CASE("a loop isn't saved over an existing one", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Taken");
    REQUIRE(als::saveLoop(makeLoop(), loopFolder).wasOk());

    SavedLoop other = makeLoop();
    other.samples.assign(10, 0.25f);
    REQUIRE(als::saveLoop(other, loopFolder).failed());

    SavedLoop loaded;
    REQUIRE(als::loadLoop(loopFolder, loaded).wasOk());
    REQUIRE(loaded.samples == makeLoop().samples);
}

TEST_CASE("an empty loop isn't saved", "[loop_file]") {
    const TemporaryFolder folder;
    const juce::File loopFolder = folder.root.getChildFile("Empty");

    SavedLoop noSamples = makeLoop();
    noSamples.samples.clear();
    REQUIRE(als::saveLoop(noSamples, loopFolder).failed());

    SavedLoop noRate = makeLoop();
    noRate.sampleRate = 0.0;
    REQUIRE(als::saveLoop(noRate, loopFolder).failed());
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

    // loop.json is fine, but the WAV is gone.
    const juce::File noAudio = folder.root.getChildFile("NoAudio");
    REQUIRE(als::saveLoop(makeLoop(), noAudio).wasOk());
    REQUIRE(noAudio.getChildFile("track1.wav").deleteFile());
    REQUIRE(als::loadLoop(noAudio, loaded).failed());

    // A failed load leaves the destination as it was.
    REQUIRE(loaded.samples == makeLoop().samples);
}

TEST_CASE("loop names become legal folder names", "[loop_file]") {
    const juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory);
    REQUIRE(als::loopFolder(root, "Bass Line") == root.getChildFile("Bass Line"));
    REQUIRE(als::loopFolder(root, "a/b:c?") == root.getChildFile("abc"));
    REQUIRE(als::loopFolder(root, "  Take 2. ") == root.getChildFile("Take 2"));
    REQUIRE(als::loopFolder(root, "..") == juce::File());
    REQUIRE(als::loopFolder(root, "   ") == juce::File());
}
