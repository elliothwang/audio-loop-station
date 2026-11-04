#include "LoopFile.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace als {

namespace {

constexpr int kFormatVersion = 1;
constexpr int kBitsPerSample = 32; // float, so samples round trip exactly
constexpr const char* kSettingsFileName = "loop.json";
constexpr const char* kTrackFileName = "track1.wav";

// WAV readers and writers count samples in an int.
bool validLength(std::int64_t length) {
    return length > 0 && length <= std::numeric_limits<int>::max();
}

bool validRate(double sampleRate) {
    return std::isfinite(sampleRate) && sampleRate > 0.0;
}

juce::Result writeSettings(const SavedLoop& loop, const juce::File& file) {
    // The var owns the object, since DynamicObject is reference counted.
    juce::var root(new juce::DynamicObject());
    juce::DynamicObject* object = root.getDynamicObject();
    object->setProperty("version", kFormatVersion);
    object->setProperty("sampleRate", loop.sampleRate);
    object->setProperty("length", static_cast<juce::int64>(loop.samples.size()));
    if (!file.replaceWithText(juce::JSON::toString(root))) {
        return juce::Result::fail("Could not write " + file.getFullPathName());
    }
    return juce::Result::ok();
}

juce::Result writeTrack(const SavedLoop& loop, const juce::File& file) {
    std::unique_ptr<juce::FileOutputStream> stream = file.createOutputStream();
    if (stream == nullptr) {
        return juce::Result::fail("Could not create " + file.getFullPathName());
    }
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(stream.get(), loop.sampleRate, 1, kBitsPerSample, {}, 0));
    if (writer == nullptr) {
        return juce::Result::fail("Could not create a WAV writer for " + file.getFileName());
    }
    stream.release(); // the writer owns the stream now

    const std::array<const float*, 1> channels{loop.samples.data()};
    const bool written =
        writer->writeFromFloatArrays(channels.data(), 1, static_cast<int>(loop.samples.size()));
    writer.reset(); // closing the writer finishes the WAV header
    if (!written) {
        return juce::Result::fail("Could not write " + file.getFullPathName());
    }
    return juce::Result::ok();
}

// Reads the track's WAV, which must be mono and match the rate and length in loop.json.
juce::Result readTrack(const juce::File& file, double sampleRate, std::int64_t length,
                       std::vector<float>& samples) {
    std::unique_ptr<juce::FileInputStream> stream = file.createInputStream();
    if (stream == nullptr) {
        return juce::Result::fail("Missing " + file.getFullPathName());
    }
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    if (reader == nullptr) {
        return juce::Result::fail(file.getFileName() + " is not a WAV file.");
    }
    // WAV headers store the rate as an integer.
    if (reader->numChannels != 1 ||
        juce::roundToInt(reader->sampleRate) != juce::roundToInt(sampleRate) ||
        reader->lengthInSamples != length) {
        return juce::Result::fail(file.getFileName() + " doesn't match the loop.");
    }

    std::vector<float> read(static_cast<std::size_t>(length));
    const std::array<float*, 1> channels{read.data()};
    if (!reader->read(channels.data(), 1, 0, static_cast<int>(length))) {
        return juce::Result::fail("Could not read " + file.getFullPathName());
    }
    samples = std::move(read);
    return juce::Result::ok();
}

} // namespace

juce::File defaultLoopsFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Audio Loop Station")
        .getChildFile("Loops");
}

juce::File loopFolder(const juce::File& root, const juce::String& name) {
    // Windows drops trailing dots and spaces, and a name of only dots would leave the root.
    const juce::String legal =
        juce::File::createLegalFileName(name).trim().trimCharactersAtEnd(". ");
    return legal.isEmpty() ? juce::File() : root.getChildFile(legal);
}

juce::Result saveLoop(const SavedLoop& loop, const juce::File& folder) {
    if (folder == juce::File()) {
        return juce::Result::fail("The loop needs a name.");
    }
    if (!validRate(loop.sampleRate) ||
        !validLength(static_cast<std::int64_t>(loop.samples.size()))) {
        return juce::Result::fail("There is no recorded audio to save.");
    }
    if (folder.exists()) {
        return juce::Result::fail("A loop called " + folder.getFileName() + " already exists.");
    }
    if (const juce::Result created = folder.createDirectory(); created.failed()) {
        return created;
    }

    juce::Result result = writeSettings(loop, folder.getChildFile(kSettingsFileName));
    if (result.wasOk()) {
        result = writeTrack(loop, folder.getChildFile(kTrackFileName));
    }
    if (result.failed()) {
        static_cast<void>(folder.deleteRecursively()); // best effort, the error matters more
    }
    return result;
}

juce::Result loadLoop(const juce::File& folder, SavedLoop& loop) {
    const juce::File settingsFile = folder.getChildFile(kSettingsFileName);
    if (!settingsFile.existsAsFile()) {
        return juce::Result::fail("No " + juce::String(kSettingsFileName) + " in " +
                                  folder.getFullPathName());
    }
    juce::var root;
    if (juce::JSON::parse(settingsFile.loadFileAsString(), root).failed() || !root.isObject()) {
        return juce::Result::fail(settingsFile.getFullPathName() + " is not valid JSON.");
    }
    if (static_cast<int>(root.getProperty("version", 0)) != kFormatVersion) {
        return juce::Result::fail(settingsFile.getFileName() + " is from an unknown version.");
    }

    SavedLoop read;
    read.sampleRate = root.getProperty("sampleRate", 0.0);
    const auto length = static_cast<std::int64_t>(root.getProperty("length", 0));
    if (!validRate(read.sampleRate) || !validLength(length)) {
        return juce::Result::fail(settingsFile.getFileName() + " has an invalid loop.");
    }
    if (const juce::Result trackRead =
            readTrack(folder.getChildFile(kTrackFileName), read.sampleRate, length, read.samples);
        trackRead.failed()) {
        return trackRead;
    }
    loop = std::move(read);
    return juce::Result::ok();
}

} // namespace als
