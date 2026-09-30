// MFR-303 - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SquelchDSP.h"

// Step data shared between the editor (writes) and the audio thread (reads).
struct SharedStep
{
    std::atomic<int>  note { 0 }, octave { 0 };
    std::atomic<bool> gate { true }, accent { false }, slide { false };

    squelch::Step load() const
    {
        return { note.load(), octave.load(), gate.load(), accent.load(), slide.load() };
    }

    void store (const squelch::Step& s)
    {
        note = s.note; octave = s.octave; gate = s.gate; accent = s.accent; slide = s.slide;
    }
};

class SquelchProcessor : public juce::AudioProcessor
{
public:
    SquelchProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MFR-303"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Pattern editing helpers (message thread)
    void loadDefaultPattern();
    void randomizePattern();
    void clearPattern();
    void shiftPattern (int direction);

    juce::AudioProcessorValueTreeState apvts;
    std::array<SharedStep, squelch::kMaxSteps> pattern;
    std::atomic<int> playingStep { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage&, bool seqMode);

    squelch::AcidVoice voice;
    squelch::StepSequencer sequencer;
    std::vector<int> heldNotes;
    bool wasSeqMode = false;

    juce::SmoothedValue<float> outGain;
    juce::Random random;

    std::atomic<float>* pWave; std::atomic<float>* pTune; std::atomic<float>* pCutoff;
    std::atomic<float>* pReso; std::atomic<float>* pEnvMod; std::atomic<float>* pDecay;
    std::atomic<float>* pAccent; std::atomic<float>* pDrive; std::atomic<float>* pVolume;
    std::atomic<float>* pSlide; std::atomic<float>* pSeq; std::atomic<float>* pSteps;
    std::atomic<float>* pRoot; std::atomic<float>* pSwing; std::atomic<float>* pGate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SquelchProcessor)
};
