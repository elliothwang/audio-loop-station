#include "LoopFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace als {

namespace {

constexpr int kFormatVersion = 2;
constexpr int kBitsPerSample = 32; // float, so samples round trip exactly
constexpr const char* kSettingsFileName = "loop.json";

// Returns the WAV name for the track at `index`, numbered from 1 like the lanes.
juce::String trackFileName(std::size_t index) {
    return "track" + juce::String(static_cast<int>(index) + 1) + ".wav";
}

// WAV readers and writers count samples in an int.
bool validLength(std::int64_t length) {
    return length > 0 && length <= std::numeric_limits<int>::max();
}

// Returns true for a finite, positive rate.
bool validRate(double sampleRate) {
    return std::isfinite(sampleRate) && sampleRate > 0.0;
}

// Reads a JSON integer into `integer`. Fractions and numbers too large for an int64 are refused,
// since converting them would truncate or be undefined.
bool readInteger(const juce::var& value, std::int64_t& integer) {
    if (!value.isInt() && !value.isInt64()) {
        return false;
    }
    integer = static_cast<std::int64_t>(value);
    return true;
}

// A take has to fit in the loop and start inside it, so it plays once per pass.
bool validTrack(const SavedTrack& track, std::int64_t length) {
    return static_cast<std::int64_t>(track.samples.size()) <= length && track.start >= 0 &&
           track.start < length;
}

// Checks everything saveLoop() refuses, before anything is written.
juce::Result validate(const SavedLoop& loop) {
    if (!validRate(loop.sampleRate) || !validLength(loop.length)) {
        return juce::Result::fail("There is no recorded audio to save.");
    }
    bool anyAudio = false;
    for (const SavedTrack& track : loop.tracks) {
        if (!validTrack(track, loop.length)) {
            return juce::Result::fail("A track doesn't fit the loop.");
        }
        anyAudio = anyAudio || !track.samples.empty();
    }
    if (!anyAudio) {
        return juce::Result::fail("There is no recorded audio to save.");
    }
    return juce::Result::ok();
}

// Writes loop.json with the rate, the length, and each track's audio flag and start.
juce::Result writeSettings(const SavedLoop& loop, const juce::File& file) {
    // The var owns the object, since DynamicObject is reference counted.
    juce::var root(new juce::DynamicObject());
    juce::DynamicObject* object = root.getDynamicObject();
    object->setProperty("version", kFormatVersion);
    object->setProperty("sampleRate", loop.sampleRate);
    object->setProperty("length", static_cast<juce::int64>(loop.length));

    // Every track is listed, so a track's place in the array is its lane.
    juce::Array<juce::var> tracks;
    for (const SavedTrack& track : loop.tracks) {
        juce::var entry(new juce::DynamicObject());
        entry.getDynamicObject()->setProperty("audio", !track.samples.empty());
        entry.getDynamicObject()->setProperty("start", static_cast<juce::int64>(track.start));
        tracks.add(entry);
    }
    object->setProperty("tracks", tracks);

    if (!file.replaceWithText(juce::JSON::toString(root))) {
        return juce::Result::fail("Could not write " + file.getFullPathName());
    }
    return juce::Result::ok();
}

// Writes `samples` as a mono 32-bit float WAV.
juce::Result writeTrack(const std::vector<float>& samples, double sampleRate,
                        const juce::File& file) {
    std::unique_ptr<juce::FileOutputStream> stream = file.createOutputStream();
    if (stream == nullptr) {
        return juce::Result::fail("Could not create " + file.getFullPathName());
    }
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(stream.get(), sampleRate, 1, kBitsPerSample, {}, 0));
    if (writer == nullptr) {
        return juce::Result::fail("Could not create a WAV writer for " + file.getFileName());
    }
    stream.release(); // the writer owns the stream now

    const std::array<const float*, 1> channels{samples.data()};
    const bool written =
        writer->writeFromFloatArrays(channels.data(), 1, static_cast<int>(samples.size()));
    writer.reset(); // closing the writer finishes the WAV header
    if (!written) {
        return juce::Result::fail("Could not write " + file.getFullPathName());
    }
    return juce::Result::ok();
}

// Reads one track's WAV, which must be mono, match the loop's rate, and be no longer than the loop.
juce::Result readTrack(const juce::File& file, double sampleRate, std::int64_t loopLength,
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
        reader->lengthInSamples <= 0 || reader->lengthInSamples > loopLength) {
        return juce::Result::fail(file.getFileName() + " doesn't match the loop.");
    }

    std::vector<float> read(static_cast<std::size_t>(reader->lengthInSamples));
    const std::array<float*, 1> channels{read.data()};
    if (!reader->read(channels.data(), 1, 0, static_cast<int>(read.size()))) {
        return juce::Result::fail("Could not read " + file.getFullPathName());
    }
    samples = std::move(read);
    return juce::Result::ok();
}

// A loop.json as read, before any audio is loaded. A track with no start has no audio.
struct Settings {
    LoopInfo info;
    std::array<std::optional<std::int64_t>, kTrackCount> starts{};
};

// Reads every track's entry into `starts`. All four must be listed, at least one must have audio,
// and each track with audio needs a start inside the `length` sample loop.
bool readTracks(const juce::var& tracks, std::int64_t length,
                std::array<std::optional<std::int64_t>, kTrackCount>& starts) {
    if (!tracks.isArray() || static_cast<std::size_t>(tracks.size()) != kTrackCount) {
        return false;
    }
    bool anyAudio = false;
    for (std::size_t i = 0; i < kTrackCount; ++i) {
        const juce::var& entry = tracks[static_cast<int>(i)];
        if (!entry.isObject()) {
            return false;
        }
        if (!static_cast<bool>(entry.getProperty("audio", false))) {
            continue;
        }
        std::int64_t start = 0;
        if (!readInteger(entry.getProperty("start", {}), start) || start < 0 || start >= length) {
            return false;
        }
        starts[i] = start;
        anyAudio = true;
    }
    return anyAudio;
}

// Reads and checks the folder's loop.json without loading any audio, so a listed loop's settings
// are known to be loadable.
juce::Result readSettings(const juce::File& folder, Settings& settings) {
    const juce::File settingsFile = folder.getChildFile(kSettingsFileName);
    if (!settingsFile.existsAsFile()) {
        return juce::Result::fail("No " + juce::String(kSettingsFileName) + " in " +
                                  folder.getFullPathName());
    }
    juce::var parsed;
    if (juce::JSON::parse(settingsFile.loadFileAsString(), parsed).failed() || !parsed.isObject()) {
        return juce::Result::fail(settingsFile.getFullPathName() + " is not valid JSON.");
    }
    if (static_cast<int>(parsed.getProperty("version", 0)) != kFormatVersion) {
        return juce::Result::fail(settingsFile.getFileName() + " is from an unknown version.");
    }

    Settings read;
    read.info.name = folder.getFileName();
    read.info.folder = folder;
    read.info.sampleRate = parsed.getProperty("sampleRate", 0.0);
    if (!validRate(read.info.sampleRate) ||
        !readInteger(parsed.getProperty("length", {}), read.info.length) ||
        !validLength(read.info.length) ||
        !readTracks(parsed.getProperty("tracks", {}), read.info.length, read.starts)) {
        return juce::Result::fail(settingsFile.getFileName() + " has an invalid loop.");
    }
    settings = std::move(read);
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
    if (const juce::Result valid = validate(loop); valid.failed()) {
        return valid;
    }
    if (folder.exists()) {
        return juce::Result::fail("A loop called " + folder.getFileName() + " already exists.");
    }
    if (const juce::Result created = folder.createDirectory(); created.failed()) {
        return created;
    }

    juce::Result result = writeSettings(loop, folder.getChildFile(kSettingsFileName));
    for (std::size_t i = 0; i < kTrackCount && result.wasOk(); ++i) {
        if (!loop.tracks[i].samples.empty()) {
            result = writeTrack(loop.tracks[i].samples, loop.sampleRate,
                                folder.getChildFile(trackFileName(i)));
        }
    }
    if (result.failed()) {
        // A half-written folder would fail to load later, so any failure removes it.
        static_cast<void>(folder.deleteRecursively());
    }
    return result;
}

std::vector<LoopInfo> listLoops(const juce::File& root) {
    std::vector<LoopInfo> loops;
    for (const juce::File& folder : root.findChildFiles(juce::File::findDirectories, false)) {
        Settings settings;
        if (readSettings(folder, settings).wasOk()) {
            loops.push_back(std::move(settings.info));
        }
    }
    std::sort(loops.begin(), loops.end(), [](const LoopInfo& a, const LoopInfo& b) {
        return a.name.compareNatural(b.name) < 0;
    });
    return loops;
}

juce::Result loadLoop(const juce::File& folder, SavedLoop& loop) {
    Settings settings;
    if (const juce::Result settingsRead = readSettings(folder, settings); settingsRead.failed()) {
        return settingsRead;
    }
    SavedLoop read;
    read.sampleRate = settings.info.sampleRate;
    read.length = settings.info.length;

    // readSettings() checked every start, and readTrack() checks each WAV fits the loop.
    for (std::size_t i = 0; i < kTrackCount; ++i) {
        if (!settings.starts[i]) {
            continue;
        }
        SavedTrack& track = read.tracks[i];
        track.start = *settings.starts[i];
        if (const juce::Result trackRead = readTrack(folder.getChildFile(trackFileName(i)),
                                                     read.sampleRate, read.length, track.samples);
            trackRead.failed()) {
            return trackRead;
        }
    }
    loop = std::move(read);
    return juce::Result::ok();
}

} // namespace als
