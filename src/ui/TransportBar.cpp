#include "TransportBar.h"

#include "LoopStationLookAndFeel.h"

#include <algorithm>
#include <initializer_list>

namespace {

constexpr int kTransportButtonSize = 48; // [px]

// Formats seconds as minutes, then seconds to one decimal place.
juce::String formatTime(double seconds) {
    // Round to tenths before splitting off minutes, so 59.96 reads as 1 minute, not 0 and 60.0.
    const int tenths = juce::roundToInt(std::max(0.0, seconds) * 10.0);
    const int minutes = tenths / 600;
    const double remainder = (tenths % 600) / 10.0;
    return juce::String(minutes) + ":" + juce::String(remainder, 1).paddedLeft('0', 4);
}

} // namespace

TransportBar::TransportBar()
    : record("Record", IconButton::Icon::Record, palette::record, true),
      play("Play", IconButton::Icon::Play, palette::accent, true),
      stop("Stop", IconButton::Icon::Stop, palette::mutedText, true) {
    appLabel.setText("AUDIO LOOP STATION", juce::dontSendNotification);
    appLabel.setFont(juce::FontOptions(11.0f));
    appLabel.setColour(juce::Label::textColourId, palette::mutedText);

    loopNameLabel.setText("Untitled loop", juce::dontSendNotification);
    loopNameLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    timeLabel.setFont(
        juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 26.0f, juce::Font::plain));
    setTime(0.0, 0.0);

    for (juce::Component* child : std::initializer_list<juce::Component*>{
             &appLabel, &loopNameLabel, &timeLabel, &record, &play, &stop, &saveLoop, &loadLoop}) {
        addAndMakeVisible(child);
    }
}

void TransportBar::setTime(double positionSeconds, double lengthSeconds) {
    timeLabel.setText(formatTime(positionSeconds) + " / " + formatTime(lengthSeconds),
                      juce::dontSendNotification);
}

void TransportBar::paint(juce::Graphics& g) {
    g.fillAll(palette::header);
    g.setColour(palette::border);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void TransportBar::resized() {
    auto area = getLocalBounds().reduced(24, 14);

    auto titles = area.removeFromLeft(170);
    appLabel.setBounds(titles.removeFromTop(titles.getHeight() / 2));
    loopNameLabel.setBounds(titles);
    area.removeFromLeft(12);

    for (IconButton* button : {&record, &play, &stop}) {
        button->setBounds(area.removeFromLeft(kTransportButtonSize)
                              .withSizeKeepingCentre(kTransportButtonSize, kTransportButtonSize));
        area.removeFromLeft(8);
    }
    area.removeFromLeft(12);

    loadLoop.setBounds(area.removeFromRight(64).withSizeKeepingCentre(64, 44));
    area.removeFromRight(8);
    saveLoop.setBounds(area.removeFromRight(64).withSizeKeepingCentre(64, 44));
    area.removeFromRight(16);
    timeLabel.setBounds(area);
}

juce::Button& TransportBar::recordButton() noexcept {
    return record;
}

juce::Button& TransportBar::playButton() noexcept {
    return play;
}

juce::Button& TransportBar::stopButton() noexcept {
    return stop;
}

juce::Button& TransportBar::saveLoopButton() noexcept {
    return saveLoop;
}

juce::Button& TransportBar::loadLoopButton() noexcept {
    return loadLoop;
}
