// MFR-303 - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String noteName (int midi) { return juce::MidiMessage::getMidiNoteName (midi, true, true, 5); }

using FloatParam = juce::AudioParameterFloat;
using Attrs = juce::AudioParameterFloatAttributes;

std::unique_ptr<FloatParam> percent (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name,
                                         juce::NormalisableRange<float> (0.0f, 1.0f), def,
                                         Attrs().withStringFromValueFunction ([] (float v, int)
                                         { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SquelchProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "wave", 1 }, "Waveform",
                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              {
                                                  if (v < 0.01f) return juce::String ("Saw");
                                                  if (v > 0.99f) return juce::String ("Square");
                                                  return juce::String (juce::roundToInt (v * 100.0f)) + "% Sqr";
                                              })));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "tune", 1 }, "Tune",
                                              juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (v, 2) + " st"; })));
    layout.add (percent ("cutoff", "Cutoff", 0.3f));
    layout.add (percent ("reso", "Resonance", 0.75f));
    layout.add (percent ("envmod", "Env Mod", 0.55f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "decay", 1 }, "Decay",
                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (juce::roundToInt (200.0f * std::pow (10.0f, v))) + " ms"; })));
    layout.add (percent ("accent", "Accent", 0.7f));
    layout.add (percent ("drive", "Drive", 0.1f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "slide", 1 }, "Slide Time",
                                              juce::NormalisableRange<float> (10.0f, 300.0f, 0.0f, 0.5f), 60.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (juce::roundToInt (v)) + " ms"; })));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "volume", 1 }, "Volume",
                                              juce::NormalisableRange<float> (-36.0f, 6.0f), -3.0f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "seq", 1 }, "Sequencer On", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "steps", 1 }, "Steps", 1, 16, 16));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "root", 1 }, "Root Note", 24, 60, 36,
                                                           juce::AudioParameterIntAttributes().withStringFromValueFunction (
                                                               [] (int v, int) { return noteName (v); })));
    layout.add (percent ("swing", "Swing", 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "gatelen", 1 }, "Gate Length",
                                              juce::NormalisableRange<float> (0.1f, 0.95f), 0.5f,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));
    return layout;
}

SquelchProcessor::SquelchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pWave   = apvts.getRawParameterValue ("wave");
    pTune   = apvts.getRawParameterValue ("tune");
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pReso   = apvts.getRawParameterValue ("reso");
    pEnvMod = apvts.getRawParameterValue ("envmod");
    pDecay  = apvts.getRawParameterValue ("decay");
    pAccent = apvts.getRawParameterValue ("accent");
    pDrive  = apvts.getRawParameterValue ("drive");
    pVolume = apvts.getRawParameterValue ("volume");
    pSlide  = apvts.getRawParameterValue ("slide");
    pSeq    = apvts.getRawParameterValue ("seq");
    pSteps  = apvts.getRawParameterValue ("steps");
    pRoot   = apvts.getRawParameterValue ("root");
    pSwing  = apvts.getRawParameterValue ("swing");
    pGate   = apvts.getRawParameterValue ("gatelen");

    heldNotes.reserve (128);
    loadDefaultPattern();
}

bool SquelchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SquelchProcessor::prepareToPlay (double sampleRate, int)
{
    voice.prepare (sampleRate);
    sequencer.reset();
    heldNotes.clear();
    outGain.reset (sampleRate, 0.02);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pVolume->load()));
    setLatencySamples (squelch::Decimator::kLatencyBase);
}

void SquelchProcessor::handleMidi (const juce::MidiMessage& m, bool seqMode)
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        const bool legato = ! heldNotes.empty();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
        heldNotes.push_back (n);
        if (! seqMode)
            voice.noteOn (n, m.getVelocity() >= 100, legato);
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        const bool wasCurrent = ! heldNotes.empty() && heldNotes.back() == n;
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
        if (! seqMode && wasCurrent)
        {
            if (heldNotes.empty())
                voice.noteOff();
            else
                voice.noteOn (heldNotes.back(), false, true);   // glide back to the held note
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
        voice.noteOff();
    }
}

void SquelchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    squelch::VoiceParams vp;
    vp.wave    = pWave->load();
    vp.tune    = pTune->load();
    vp.cutoff  = pCutoff->load();
    vp.reso    = pReso->load();
    vp.envMod  = pEnvMod->load();
    vp.decay   = pDecay->load();
    vp.accent  = pAccent->load();
    vp.drive   = pDrive->load();
    vp.slideMs = pSlide->load();
    voice.setParams (vp);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pVolume->load()));

    const bool seqMode = pSeq->load() > 0.5f;
    if (seqMode != wasSeqMode)
    {
        voice.noteOff();
        sequencer.reset();
        wasSeqMode = seqMode;
    }

    // Host transport
    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto b = pos->getBpm()) bpm = *b;
        }
    const double ppqPerSample = bpm / 60.0 / getSampleRate();

    std::array<squelch::Step, squelch::kMaxSteps> steps;
    for (size_t i = 0; i < steps.size(); ++i)
        steps[i] = pattern[i].load();
    const int numSteps = (int) pSteps->load();
    const int root     = (int) pRoot->load();
    const float swing  = pSwing->load();
    const float gate   = pGate->load();

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto midiIt = midi.cbegin();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
            handleMidi ((*midiIt).getMessage(), seqMode);

        if (seqMode)
        {
            // A held key transposes the pattern relative to middle C (C5 in FL).
            const int transpose = heldNotes.empty() ? 0 : heldNotes.back() - 60;
            sequencer.tick (voice, playing, ppq + i * ppqPerSample, steps.data(), numSteps,
                            root + transpose, swing, gate);
        }

        const float s = voice.renderSample() * outGain.getNextValue();
        left[i] = s;
        if (right != nullptr)
            right[i] = s;
    }

    playingStep = seqMode ? sequencer.getCurrentStep() : -1;
    midi.clear();
}

//==============================================================================
void SquelchProcessor::loadDefaultPattern()
{
    const squelch::Step def[squelch::kMaxSteps] = {
        { 0, 0, true, true, false },  { 0, 0, true, false, false }, { 0, 1, true, false, true },
        { 3, 0, true, false, false }, { 0, 0, false, false, false }, { 0, 0, true, true, false },
        { 7, 0, true, false, true },  { 10, 0, true, false, false }, { 0, 0, true, false, false },
        { 0, 1, true, true, false },  { 0, 0, false, false, false }, { 5, 0, true, false, true },
        { 7, 0, true, false, false }, { 0, 0, true, true, false },  { 3, 1, true, false, true },
        { 0, 0, true, false, false },
    };
    for (size_t i = 0; i < pattern.size(); ++i)
        pattern[i].store (def[i]);
}

void SquelchProcessor::randomizePattern()
{
    static const int scale[] = { 0, 0, 0, 3, 5, 7, 7, 10, 12 };
    for (auto& s : pattern)
    {
        squelch::Step st;
        st.gate   = random.nextFloat() < 0.8f;
        st.note   = scale[random.nextInt ((int) std::size (scale))];
        const float o = random.nextFloat();
        st.octave = o < 0.15f ? -1 : (o > 0.8f ? 1 : 0);
        st.accent = random.nextFloat() < 0.3f;
        st.slide  = random.nextFloat() < 0.22f;
        s.store (st);
    }
}

void SquelchProcessor::clearPattern()
{
    for (auto& s : pattern)
        s.store ({ 0, 0, true, false, false });
}

void SquelchProcessor::shiftPattern (int direction)
{
    const int n = juce::jlimit (1, squelch::kMaxSteps, (int) pSteps->load());
    std::vector<squelch::Step> tmp ((size_t) n);
    for (int i = 0; i < n; ++i)
        tmp[(size_t) ((i + direction + n) % n)] = pattern[(size_t) i].load();
    for (int i = 0; i < n; ++i)
        pattern[(size_t) i].store (tmp[(size_t) i]);
}

//==============================================================================
void SquelchProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PATTERN"), nullptr);

    juce::ValueTree pat ("PATTERN");
    for (auto& s : pattern)
    {
        const auto st = s.load();
        juce::ValueTree step ("STEP");
        step.setProperty ("note", st.note, nullptr);
        step.setProperty ("oct", st.octave, nullptr);
        step.setProperty ("gate", st.gate, nullptr);
        step.setProperty ("acc", st.accent, nullptr);
        step.setProperty ("slide", st.slide, nullptr);
        pat.appendChild (step, nullptr);
    }
    state.appendChild (pat, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SquelchProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    if (auto pat = state.getChildWithName ("PATTERN"); pat.isValid())
    {
        for (int i = 0; i < juce::jmin (pat.getNumChildren(), squelch::kMaxSteps); ++i)
        {
            auto step = pat.getChild (i);
            pattern[(size_t) i].store ({ juce::jlimit (0, 12, (int) step.getProperty ("note", 0)),
                                         juce::jlimit (-1, 1, (int) step.getProperty ("oct", 0)),
                                         (bool) step.getProperty ("gate", true),
                                         (bool) step.getProperty ("acc", false),
                                         (bool) step.getProperty ("slide", false) });
        }
        state.removeChild (pat, nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* SquelchProcessor::createEditor() { return new SquelchEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SquelchProcessor(); }
