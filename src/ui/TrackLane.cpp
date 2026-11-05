#include "TrackLane.h"

#include "LoopStationLookAndFeel.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <initializer_list>

namespace {

constexpr int kSlipStepMs = 10;          // [ms] per button press
constexpr float kWaveformHeight = 64.0f; // [px] of the empty waveform outline
constexpr int kSmallButtonSize = 28;     // [px] square, for delete and the slip buttons
constexpr int kRowGap = 8;               // [px] between the header's rows
constexpr int kColumnGap = 14;           // [px] between the header's right-hand columns

// Gives a short-labelled button a full name for screen readers and a matching tooltip.
void nameButton(juce::Button& button, const juce::String& name) {
    button.setTitle(name);
    button.setTooltip(name);
}

// Styles a label as small grey caption text.
void styleCaption(juce::Label& label, const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(11.0f));
    label.setColour(juce::Label::textColourId, palette::mutedText);
}

} // namespace

TrackLane::TrackLane(int trackNumber)
    : deleteButton("Delete track " + juce::String(trackNumber), IconButton::Icon::Delete,
                   palette::mutedText, false) {
    numberLabel.setText(juce::String(trackNumber).paddedLeft('0', 2), juce::dontSendNotification);
    numberLabel.setFont(juce::FontOptions(12.0f));
    numberLabel.setColour(juce::Label::textColourId, palette::mutedText);

    nameLabel.setText("Track " + juce::String(trackNumber), juce::dontSendNotification);
    nameLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

    stateLabel.setText("EMPTY", juce::dontSendNotification);
    stateLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setColour(juce::Label::backgroundColourId, palette::control);
    stateLabel.setColour(juce::Label::textColourId, palette::mutedText);

    styleCaption(volumeLabel, "Vol");
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(1.0, juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, true, 56, 20);
    volumeSlider.textFromValueFunction = [](double gain) {
        return juce::Decibels::toString(juce::Decibels::gainToDecibels(gain), 1);
    };
    volumeSlider.updateText(); // the text box was filled before the dB format was set
    volumeSlider.setTitle("Track " + juce::String(trackNumber) + " volume");

    // When toggled on, mute turns yellow and reverse turns blue.
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonOnColourId, palette::mute);
    reverseButton.setClickingTogglesState(true);
    reverseButton.setColour(juce::TextButton::buttonOnColourId, palette::reverse);

    const juce::String track = "track " + juce::String(trackNumber);
    nameButton(muteButton, "Mute " + track);
    nameButton(reverseButton, "Reverse " + track);
    nameButton(slipEarlierButton, "Slip " + track + " earlier");
    nameButton(slipLaterButton, "Slip " + track + " later");

    // "-" moves the track earlier, "+" later.
    slipEarlierButton.onClick = [this]() { nudgeSlip(-kSlipStepMs); };
    slipLaterButton.onClick = [this]() { nudgeSlip(kSlipStepMs); };
    slipOffsetLabel.setText("0 ms", juce::dontSendNotification);
    slipOffsetLabel.setFont(juce::FontOptions(11.0f));
    slipOffsetLabel.setJustificationType(juce::Justification::centred);

    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setDoubleClickReturnValue(true, 0.0);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    panSlider.setTitle("Track " + juce::String(trackNumber) + " pan");
    panSlider.onValueChange = [this]() { updatePanLabel(); };
    panLabel.setJustificationType(juce::Justification::centred);
    styleCaption(panLabel, {});
    updatePanLabel();

    for (juce::Component* child : std::initializer_list<juce::Component*>{
             &numberLabel, &nameLabel, &stateLabel, &volumeLabel, &volumeSlider, &muteButton,
             &reverseButton, &slipEarlierButton, &slipLaterButton, &slipOffsetLabel, &panSlider,
             &panLabel, &meter, &deleteButton}) {
        addAndMakeVisible(child);
    }
}

void TrackLane::paint(juce::Graphics& g) {
    g.fillAll(palette::lane);
    g.setColour(palette::border);
    g.fillRect(kHeaderWidth - 1, 0, 1, getHeight());
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    const auto lane = getLocalBounds().withTrimmedLeft(kHeaderWidth).toFloat();
    const auto waveformArea =
        lane.reduced(8.0f, 0.0f).withSizeKeepingCentre(lane.getWidth() - 16.0f, kWaveformHeight);
    if (thumbnail.getTotalLength() > 0.0) {
        g.setColour(palette::meterTrack);
        g.fillRoundedRectangle(waveformArea, 6.0f);
        g.setColour(palette::accent);
        thumbnail.drawChannel(g, waveformArea.toNearestInt(), 0.0, thumbnail.getTotalLength(), 0,
                              1.0f);
        return;
    }

    juce::Path outline;
    outline.addRoundedRectangle(waveformArea, 6.0f);
    juce::Path dashedOutline;
    const float dashLengths[] = {4.0f, 4.0f};
    juce::PathStrokeType(1.0f).createDashedStroke(dashedOutline, outline, dashLengths, 2);
    g.setColour(palette::controlBorder);
    g.fillPath(dashedOutline);

    // The waveform replaces this text once the track has a clip.
    const char* text = state == als::TrackState::Empty       ? "Empty. Press Record to add a take."
                       : state == als::TrackState::Recording ? "Recording"
                                                             : "Recorded";
    g.setColour(palette::mutedText);
    g.setFont(13.0f);
    g.drawText(text, waveformArea, juce::Justification::centred);
}

void TrackLane::resized() {
    auto header = getLocalBounds().withWidth(kHeaderWidth).reduced(16, 12);

    // The right side holds delete, the meter, and the pan knob, each a full-height column.
    auto deleteColumn = header.removeFromRight(kSmallButtonSize);
    deleteButton.setBounds(deleteColumn.removeFromBottom(kSmallButtonSize));
    header.removeFromRight(kRowGap);
    meter.setBounds(header.removeFromRight(8));
    header.removeFromRight(kColumnGap);
    auto knobColumn = header.removeFromRight(56);
    panLabel.setBounds(knobColumn.removeFromBottom(14));
    panSlider.setBounds(knobColumn.withSizeKeepingCentre(46, 46));
    header.removeFromRight(kColumnGap);

    auto titleRow = header.removeFromTop(22);
    numberLabel.setBounds(titleRow.removeFromLeft(26));
    stateLabel.setBounds(titleRow.removeFromRight(64).withSizeKeepingCentre(64, 18));
    nameLabel.setBounds(titleRow);
    header.removeFromTop(kRowGap);

    auto volumeRow = header.removeFromTop(24);
    volumeLabel.setBounds(volumeRow.removeFromLeft(30));
    volumeSlider.setBounds(volumeRow);
    header.removeFromTop(kRowGap);

    auto buttonRow = header.removeFromTop(kSmallButtonSize);
    muteButton.setBounds(buttonRow.removeFromLeft(34));
    buttonRow.removeFromLeft(6);
    reverseButton.setBounds(buttonRow.removeFromLeft(44));
    buttonRow.removeFromLeft(12);
    slipEarlierButton.setBounds(buttonRow.removeFromLeft(kSmallButtonSize));
    slipLaterButton.setBounds(buttonRow.removeFromRight(kSmallButtonSize));
    slipOffsetLabel.setBounds(buttonRow);
}

void TrackLane::setState(als::TrackState newState) {
    if (newState == state) {
        return;
    }
    state = newState;
    switch (state) {
    case als::TrackState::Empty:
        stateLabel.setText("EMPTY", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId, palette::control);
        stateLabel.setColour(juce::Label::textColourId, palette::mutedText);
        break;
    case als::TrackState::Recording:
        stateLabel.setText("REC", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId, palette::record);
        stateLabel.setColour(juce::Label::textColourId, palette::text);
        break;
    case als::TrackState::Playing:
        stateLabel.setText("LOOP", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId, palette::accent);
        stateLabel.setColour(juce::Label::textColourId, palette::background);
        break;
    }
    repaint();
}

void TrackLane::setWaveform(const juce::AudioBuffer<float>& clip, double sampleRate) {
    // The whole clip goes in at once, so the thumbnail is complete before the next paint.
    thumbnail.reset(1, sampleRate, clip.getNumSamples());
    thumbnail.addBlock(0, clip, 0, clip.getNumSamples());
    repaint();
}

void TrackLane::clearWaveform() {
    thumbnail.clear();
    repaint();
}

void TrackLane::nudgeSlip(int deltaMs) {
    slipOffsetMs += deltaMs;

    // Show a sign on later offsets too, so +10 and -10 are easy to tell apart.
    const juce::String sign = slipOffsetMs > 0 ? "+" : "";
    slipOffsetLabel.setText(sign + juce::String(slipOffsetMs) + " ms", juce::dontSendNotification);
}

void TrackLane::updatePanLabel() {
    const double pan = panSlider.getValue();
    const int amount = juce::roundToInt(std::abs(pan) * 100.0);
    const juce::String position =
        amount == 0 ? juce::String("C") : juce::String(amount) + (pan < 0.0 ? "L" : "R");
    panLabel.setText("Pan " + position, juce::dontSendNotification);
}
