// MFR-303 - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
} // namespace

//==============================================================================
SquelchLookAndFeel::SquelchLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::cellOff);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void SquelchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, endAngle, true);
    g.setColour (colours::edge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, angle, true);
    g.setColour (colours::acid);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4e55), centre.x, centre.y - knobR,
                                             juce::Colour (0xff17191c), centre.x, centre.y + knobR, false));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void SquelchLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = colours::cellOff;
    if (down) c = c.brighter (0.3f);
    else if (highlighted) c = c.brighter (0.12f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void SquelchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::acid : (highlighted ? colours::cellOff.brighter (0.12f) : colours::cellOff));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::bg : colours::text);
    g.setFont (font (13.0f, true));
    g.drawText (b.getButtonText() + (on ? "  ON" : "  OFF"), r, juce::Justification::centred);
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    fineDrag = e.mods.isCtrlDown() || e.mods.isShiftDown();
    lastPos = e.position;
    Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! fineDrag || ! isEnabled())
    {
        Slider::mouseDrag (e);
        return;
    }

    // Linear fine mode: 2000 px of drag covers the whole range (8x finer than normal).
    const float delta = (e.position.x - lastPos.x) + (lastPos.y - e.position.y);
    lastPos = e.position;
    const double pos = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta / 2000.0);
    setValue (proportionOfLengthToValue (pos), juce::sendNotificationSync);
}

double FineSlider::snapValue (double attempted, DragMode mode)
{
    if (detentWidth > 0.0 && ! fineDrag && mode != notDragging && std::abs (attempted - detentValue) < detentWidth)
        return detentValue;
    return attempted;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& title)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));   // also Alt-click
    if (id == "tune")
        slider.setDetent (0.0, 0.25);
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    label.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
StepGrid::StepGrid (SquelchProcessor& p) : proc (p) {}

juce::Rectangle<float> StepGrid::cell (int col, int row) const
{
    const float colW = (getWidth() - labelWidth) / (float) squelch::kMaxSteps;
    float y = 0.0f;
    for (int r = 0; r < row; ++r)
        y += rowHeights[(size_t) r] + 4.0f;
    return { labelWidth + col * colW, y, colW, rowHeights[(size_t) row] };
}

bool StepGrid::hitCell (juce::Point<float> pt, int& col, int& row) const
{
    for (int r = 0; r < numRows; ++r)
        for (int c = 0; c < squelch::kMaxSteps; ++c)
            if (cell (c, r).contains (pt))
            {
                col = c;
                row = r;
                return true;
            }
    return false;
}

bool StepGrid::getToggle (int col, int row) const
{
    const auto& s = proc.pattern[(size_t) col];
    switch (row)
    {
        case rowGate:   return s.gate;
        case rowAccent: return s.accent;
        case rowSlide:  return s.slide;
        case rowUp:     return s.octave == 1;
        case rowDown:   return s.octave == -1;
        default:        return false;
    }
}

void StepGrid::applyToggle (int col, int row, bool value)
{
    auto& s = proc.pattern[(size_t) col];
    switch (row)
    {
        case rowGate:   s.gate = value; break;
        case rowAccent: s.accent = value; break;
        case rowSlide:  s.slide = value; break;
        case rowUp:     s.octave = value ? 1 : (s.octave == 1 ? 0 : s.octave.load()); break;
        case rowDown:   s.octave = value ? -1 : (s.octave == -1 ? 0 : s.octave.load()); break;
        default: break;
    }
}

void StepGrid::paint (juce::Graphics& g)
{
    static const char* const labels[] = { "", "NOTE", "UP", "DOWN", "GATE", "ACCENT", "SLIDE" };
    const int numSteps = (int) proc.apvts.getRawParameterValue ("steps")->load();
    const int playing  = proc.playingStep.load();

    for (int r = 0; r < numRows; ++r)
    {
        const auto first = cell (0, r);
        g.setColour (colours::dimText);
        g.setFont (font (11.0f, true));
        g.drawText (labels[r], juce::Rectangle<float> (0, first.getY(), labelWidth - 6, first.getHeight()),
                    juce::Justification::centredRight);

        for (int c = 0; c < squelch::kMaxSteps; ++c)
        {
            const auto rc = cell (c, r).reduced (2.0f, 0.0f);
            const bool active = c < numSteps;
            const float alpha = active ? 1.0f : 0.3f;
            const auto s = proc.pattern[(size_t) c].load();

            if (r == rowLed)
            {
                const auto dot = juce::Rectangle<float> (8, 8).withCentre (rc.getCentre());
                g.setColour (c == playing ? colours::led : colours::cellOff.withAlpha (alpha));
                g.fillEllipse (dot);
                if (c % 4 == 0)
                {
                    g.setColour (colours::dimText.withAlpha (alpha));
                    g.setFont (font (10.0f));
                    g.drawText (juce::String (c + 1), rc.withWidth (14.0f), juce::Justification::centredLeft);
                }
                continue;
            }

            if (r == rowNote)
            {
                g.setColour ((s.gate ? colours::panel.brighter (0.15f) : colours::cellOff).withAlpha (alpha));
                g.fillRoundedRectangle (rc, 3.0f);
                const float fill = s.note / 12.0f;
                g.setColour (colours::acid.withAlpha (0.25f * alpha));
                g.fillRoundedRectangle (rc.withTop (rc.getBottom() - rc.getHeight() * fill), 3.0f);
                g.setColour ((s.gate ? colours::text : colours::dimText).withAlpha (alpha));
                g.setFont (font (13.0f, true));
                juce::String name (noteNames[s.note]);
                if (s.note == 12) name << "'";
                g.drawText (name, rc, juce::Justification::centred);
                continue;
            }

            const bool on = getToggle (c, r);
            auto onColour = r == rowGate ? colours::text : (r == rowAccent ? colours::led : colours::acid);
            g.setColour ((on ? onColour : colours::cellOff).withAlpha (alpha));
            g.fillRoundedRectangle (rc, 3.0f);
        }
    }

    // Beat separators
    g.setColour (colours::edge);
    for (int c = 4; c < squelch::kMaxSteps; c += 4)
    {
        const float x = cell (c, 0).getX();
        g.drawLine (x, 0.0f, x, (float) getHeight(), 1.0f);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int col, row;
    dragRow = -1;
    if (! hitCell (e.position, col, row) || row == rowLed)
        return;

    dragRow = row;
    dragCol = col;
    if (row == rowNote)
    {
        dragStartNote = proc.pattern[(size_t) col].note;
        if (e.mods.isRightButtonDown())
            proc.pattern[(size_t) col].gate = ! proc.pattern[(size_t) col].gate;
    }
    else
    {
        dragValue = ! getToggle (col, row);
        applyToggle (col, row, dragValue);
    }
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow == rowNote)
    {
        const int n = juce::jlimit (0, 12, dragStartNote - juce::roundToInt (e.getDistanceFromDragStartY() / 8.0f));
        proc.pattern[(size_t) dragCol].note = n;
        repaint();
    }
    else if (dragRow > rowNote)
    {
        // Paint the same value across columns in this row.
        const float colW = (getWidth() - labelWidth) / (float) squelch::kMaxSteps;
        const int col = (int) std::floor ((e.position.x - labelWidth) / colW);
        if (col >= 0 && col < squelch::kMaxSteps)
        {
            applyToggle (col, dragRow, dragValue);
            repaint();
        }
    }
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int col, row;
    if (hitCell (e.position, col, row) && row == rowNote)
    {
        auto& n = proc.pattern[(size_t) col].note;
        n = juce::jlimit (0, 12, n.load() + (w.deltaY > 0 ? 1 : -1));
        repaint();
    }
}

//==============================================================================
SquelchEditor::SquelchEditor (SquelchProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      seqAttachment (p.apvts, "seq", seqToggle),
      grid (p)
{
    setLookAndFeel (&lnf);

    const std::pair<const char*, const char*> synth[] = {
        { "tune", "Tune" },     { "wave", "Wave" },     { "cutoff", "Cutoff" },
        { "reso", "Reso" },     { "envmod", "Env Mod" }, { "decay", "Decay" },
        { "accent", "Accent" }, { "slide", "Slide" },   { "drive", "Drive" },
        { "volume", "Volume" },
    };
    for (auto& [id, name] : synth)
        addAndMakeVisible (*synthKnobs.emplace_back (std::make_unique<Knob> (p.apvts, id, name)));

    const std::pair<const char*, const char*> seq[] = {
        { "root", "Root" }, { "steps", "Steps" }, { "swing", "Swing" }, { "gatelen", "Gate" },
    };
    for (auto& [id, name] : seq)
        addAndMakeVisible (*seqKnobs.emplace_back (std::make_unique<Knob> (p.apvts, id, name)));

    addAndMakeVisible (seqToggle);
    for (auto* b : { &randomBtn, &clearBtn, &defaultBtn, &leftBtn, &rightBtn })
        addAndMakeVisible (b);

    randomBtn.onClick  = [this] { proc.randomizePattern(); grid.repaint(); };
    clearBtn.onClick   = [this] { proc.clearPattern(); grid.repaint(); };
    defaultBtn.onClick = [this] { proc.loadDefaultPattern(); grid.repaint(); };
    leftBtn.onClick    = [this] { proc.shiftPattern (-1); grid.repaint(); };
    rightBtn.onClick   = [this] { proc.shiftPattern (1); grid.repaint(); };

    addAndMakeVisible (grid);

    setSize (960, 530);
    startTimerHz (30);
}

SquelchEditor::~SquelchEditor()
{
    setLookAndFeel (nullptr);
}

void SquelchEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bg.brighter (0.06f), 0, 0, colours::bg, 0, (float) getHeight(), false));
    g.fillAll();

    // Title
    g.setColour (colours::acid);
    g.setFont (font (30.0f, true));
    g.drawText ("MFR-303", 24, 12, 300, 36, juce::Justification::centredLeft);
    g.setColour (colours::dimText);
    g.setFont (font (12.0f));
    g.drawText ("ACID BASS LINE SYNTHESIZER", 170, 14, 300, 18, juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    g.drawText ("v" JucePlugin_VersionString "  |  musicforrobots.com", 170, 30, 300, 14, juce::Justification::centredLeft);

    // Panels
    for (auto r : { juce::Rectangle<float> (16, 58, (float) getWidth() - 32, 130),
                    juce::Rectangle<float> (16, 200, (float) getWidth() - 32, (float) getHeight() - 216) })
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }

    g.setColour (colours::dimText);
    g.setFont (font (10.0f));
    g.drawText ("Knobs: double-click or Alt-click resets, Ctrl/Shift-drag for fine control, click the value to type one. "
                "Grid: drag NOTE up/down or scroll, right-click NOTE toggles gate, drag across buttons to paint.",
                30, getHeight() - 34, getWidth() - 60, 14, juce::Justification::centredLeft);
}

void SquelchEditor::resized()
{
    seqToggle.setBounds (getWidth() - 176, 16, 156, 30);

    auto knobRow = juce::Rectangle<int> (26, 66, getWidth() - 52, 116);
    const int kw = knobRow.getWidth() / (int) synthKnobs.size();
    for (auto& k : synthKnobs)
        k->setBounds (knobRow.removeFromLeft (kw).reduced (2, 0));

    auto seqArea = juce::Rectangle<int> (26, 210, getWidth() - 52, getHeight() - 250);
    auto side = seqArea.removeFromLeft (170);
    auto knobs = side.removeFromTop (180);
    for (int i = 0; i < (int) seqKnobs.size(); ++i)
        seqKnobs[(size_t) i]->setBounds (knobs.getX() + (i % 2) * 85, knobs.getY() + (i / 2) * 90, 80, 88);

    side.removeFromTop (6);
    auto row1 = side.removeFromTop (26);
    randomBtn.setBounds (row1.removeFromLeft (82));
    row1.removeFromLeft (6);
    clearBtn.setBounds (row1);
    side.removeFromTop (6);
    auto row2 = side.removeFromTop (26);
    defaultBtn.setBounds (row2.removeFromLeft (82));
    row2.removeFromLeft (6);
    leftBtn.setBounds (row2.removeFromLeft (38));
    row2.removeFromLeft (6);
    rightBtn.setBounds (row2);

    seqArea.removeFromLeft (14);
    grid.setBounds (seqArea.withTrimmedTop (4));
}

void SquelchEditor::timerCallback()
{
    grid.repaint();
}
