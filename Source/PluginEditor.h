// MFR-303 - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace colours
{
const juce::Colour bg      { 0xff1b1d20 };
const juce::Colour panel   { 0xff25282c };
const juce::Colour edge    { 0xff3a3e44 };
const juce::Colour text    { 0xffd8d4cc };
const juce::Colour dimText { 0xff8a8780 };
const juce::Colour acid    { 0xffffb000 };
const juce::Colour led     { 0xffff3b2f };
const juce::Colour cellOff { 0xff30343a };
} // namespace colours

class SquelchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SquelchLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Rotary slider with Ctrl/Shift fine-drag and an optional snap detent at a centre value.
class FineSlider : public juce::Slider
{
public:
    void setDetent (double value, double width) { detentValue = value; detentWidth = width; }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    double snapValue (double attempted, DragMode) override;

private:
    bool fineDrag = false;
    juce::Point<float> lastPos;
    double detentValue = 0.0, detentWidth = 0.0;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title);
    void resized() override;

    FineSlider slider;

private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class StepGrid : public juce::Component
{
public:
    explicit StepGrid (SquelchProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    enum Row { rowLed, rowNote, rowUp, rowDown, rowGate, rowAccent, rowSlide, numRows };

private:
    juce::Rectangle<float> cell (int col, int row) const;
    bool hitCell (juce::Point<float>, int& col, int& row) const;
    void applyToggle (int col, int row, bool value);
    bool getToggle (int col, int row) const;

    SquelchProcessor& proc;
    static constexpr float labelWidth = 58.0f;
    std::array<float, numRows> rowHeights { 14.0f, 60.0f, 24.0f, 24.0f, 30.0f, 30.0f, 30.0f };

    int dragRow = -1, dragCol = -1, dragStartNote = 0;
    bool dragValue = false;
};

class SquelchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SquelchEditor (SquelchProcessor&);
    ~SquelchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SquelchProcessor& proc;
    SquelchLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> synthKnobs, seqKnobs;
    juce::ToggleButton seqToggle { "SEQUENCER" };
    juce::AudioProcessorValueTreeState::ButtonAttachment seqAttachment;
    juce::TextButton randomBtn { "Random" }, clearBtn { "Clear" }, defaultBtn { "Init" },
                     leftBtn { "<" }, rightBtn { ">" };
    StepGrid grid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SquelchEditor)
};
