#pragma once

// Saves and loads a loop as a folder. loop.json holds the format version, sample rate, and
// length, and track1.wav beside it holds the track as 32-bit float.

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdint>
#include <vector>

namespace als {

// A loop as it's stored, with one mono track.
struct SavedLoop {
    double sampleRate = 0.0;
    std::vector<float> samples;
};

// A saved loop's name and size, read from its loop.json without loading the audio.
struct LoopInfo {
    juce::String name; // the folder's name
    juce::File folder;
    double sampleRate = 0.0;
    std::int64_t length = 0; // [samples]
};

// Returns the user's Documents/Audio Loop Station/Loops, where loops are saved locally.
[[nodiscard]] juce::File defaultLoopsFolder();

// Returns the folder for a loop called `name` inside `root`, with characters the file system
// can't take removed. Returns an empty File if nothing is left of the name.
[[nodiscard]] juce::File loopFolder(const juce::File& root, const juce::String& name);

// Writes `loop` to a new `folder`. Fails if the folder already exists, the loop has no samples or
// rate, or a write fails, in which case nothing is left behind.
[[nodiscard]] juce::Result saveLoop(const SavedLoop& loop, const juce::File& folder);

// Returns every loop in `root`, sorted by name. Folders without a valid loop.json are skipped,
// so the list only offers loops that can be loaded.
[[nodiscard]] std::vector<LoopInfo> listLoops(const juce::File& root);

// Reads the loop in `folder` into `loop`. Fails if loop.json is missing or invalid, or the WAV
// is missing or doesn't match it. `loop` is untouched on failure.
[[nodiscard]] juce::Result loadLoop(const juce::File& folder, SavedLoop& loop);

} // namespace als
