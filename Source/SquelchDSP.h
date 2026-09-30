// MFR-303 - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
// MFR-303 voice: oscillator, filter, envelopes and step sequencer.
// Pure C++ (no JUCE) so it can be exercised by the offline test tool.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace squelch
{

constexpr int   kOversample = 4;
constexpr float kPi         = 3.14159265358979f;

// Rational tanh approximation, exact at +-3 and clamped beyond.
inline float fastTanh (float x)
{
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

//==============================================================================
// Linear-phase FIR that takes kOversample samples in and gives one out.
class Decimator
{
public:
    static constexpr int kTaps = 257;                          // odd, symmetric
    static constexpr int kLatencyBase = (kTaps - 1) / 2 / kOversample;  // 32 samples

    void init()
    {
        const double fc = 0.5 / kOversample * 0.96;            // cycles per OS sample
        const int    m  = kTaps - 1;
        double sum = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const double x    = n - m * 0.5;
            const double sinc = x == 0.0 ? 2.0 * fc
                                         : std::sin (2.0 * 3.141592653589793 * fc * x) / (3.141592653589793 * x);
            const double w    = 0.42 - 0.5 * std::cos (2.0 * 3.141592653589793 * n / m)
                                     + 0.08 * std::cos (4.0 * 3.141592653589793 * n / m);
            taps[(size_t) n] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (auto& t : taps)
            t = (float) (t / sum);
        reset();
    }

    void reset()
    {
        buffer.fill (0.0f);
        pos = 0;
    }

    float process (const float* in)
    {
        for (int i = 0; i < kOversample; ++i)
        {
            buffer[(size_t) pos] = buffer[(size_t) (pos + kTaps)] = in[i];
            if (++pos == kTaps)
                pos = 0;
        }
        const float* p = buffer.data() + pos;
        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k)
            acc += p[k] * taps[(size_t) k];
        return acc;
    }

private:
    std::array<float, kTaps>     taps {};
    std::array<float, kTaps * 2> buffer {};
    int pos = 0;
};

//==============================================================================
// Four coupled one-pole stages (diode-ladder topology) with a saturating input
// and a highpass in the resonance feedback path, which is what thins the low
// end as resonance goes up on the real thing.
class DiodeLadder
{
public:
    // Calibrated with tools/SquelchTest so the resonant peak lands on fc.
    static constexpr float kCutoffCal  = 0.62f;
    static constexpr float kMaxFeedback = 21.5f;

    void prepare (float oversampledRate)
    {
        fsOS   = oversampledRate;
        hpCoef = 1.0f - std::exp (-2.0f * kPi * 150.0f / fsOS);
        reset();
    }

    void reset() { y1 = y2 = y3 = y4 = hpState = 0.0f; }

    void setCutoff (float hz)
    {
        const float w = std::min (kPi * hz / fsOS, 1.2f);
        b = std::clamp (kCutoffCal * 2.0f * std::tan (w) / (1.0f + std::tan (w)), 1.0e-5f, 0.45f);
    }

    void setFeedback (float feedback) { k = feedback; }

    float process (float x)
    {
        hpState += hpCoef * (y4 - hpState);
        const float y0 = fastTanh (x - k * (y4 - hpState));

        y1 += 2.0f * b * (y0 - y1 + y2);
        y2 += b * (y1 - 2.0f * y2 + y3);
        y3 += b * (y2 - 2.0f * y3 + y4);
        y4 += b * (y3 - 2.0f * y4);
        return y4;
    }

private:
    float fsOS = 192000.0f, hpCoef = 0.0f;
    float b = 0.1f, k = 0.0f;
    float y1 = 0, y2 = 0, y3 = 0, y4 = 0, hpState = 0;
};

//==============================================================================
struct VoiceParams
{
    float wave    = 0.0f;    // 0 = saw, 1 = square
    float tune    = 0.0f;    // semitones
    float cutoff  = 0.35f;   // 0..1
    float reso    = 0.7f;    // 0..1
    float envMod  = 0.5f;    // 0..1
    float decay   = 0.4f;    // 0..1
    float accent  = 0.6f;    // 0..1
    float drive   = 0.15f;   // 0..1
    float slideMs = 60.0f;
};

class AcidVoice
{
public:
    void prepare (double sampleRate)
    {
        fs   = (float) sampleRate;
        fsOS = fs * kOversample;
        filter.prepare (fsOS);
        decimator.init();

        attackK  = coefForTau (0.002f);
        releaseK = coefForTau (0.006f);
        holdK    = 1.0f - coefForTau (3.0f);
        megAccK  = 1.0f - coefForTau (0.2f / 3.0f);
        outHpK   = 1.0f - std::exp (-2.0f * kPi * 25.0f / fs);
        setParams (params);
        reset();
    }

    void reset()
    {
        filter.reset();
        decimator.reset();
        phase = 0.0f;
        meg = amp = ampHold = accCap = outHpState = 0.0f;
        gate = sliding = accentNote = false;
    }

    void setParams (const VoiceParams& p)
    {
        params = p;
        const float decaySec = 0.2f * std::pow (10.0f, p.decay);   // 0.2 s .. 2 s
        megNormK = 1.0f - coefForTau (decaySec / 3.0f);
        glideK   = coefForTau (std::max (p.slideMs, 1.0f) * 0.001f / 3.0f);

        // Higher resonance smooths the accent sweep more (ganged pot on the original).
        accChargeK = 1.0f / ((0.08f + 0.10f * p.reso) * fs);
        accDisK    = coefForTau (0.12f + 0.25f * p.reso);

        const float r = p.reso;
        filter.setFeedback (DiodeLadder::kMaxFeedback * r * (0.6f + 0.4f * r));
        driveGain = 1.0f + 24.0f * p.drive * p.drive;
    }

    void noteOn (int note, bool accent, bool slide)
    {
        target = (float) note;
        if (slide && gate)
        {
            sliding    = true;
            accentNote = accent;
            return;
        }
        sliding    = false;
        pitch      = target;
        gate       = true;
        accentNote = accent;
        meg        = 1.0f;
        ampHold    = 1.0f;
    }

    void noteOff() { gate = false; }

    bool isGateOn() const { return gate; }

    float renderSample()
    {
        const auto& p = params;

        // --- envelopes (base rate) ---
        meg *= accentNote ? megAccK : megNormK;
        accCap += (accentNote ? meg * accChargeK : 0.0f) - accCap * accDisK;
        const float accSweep = fastTanh (accCap * 1.6f);

        if (gate)
        {
            amp += (1.0f - amp) * attackK;
            ampHold *= holdK;
        }
        else
        {
            amp -= amp * releaseK;
        }

        if (sliding)
            pitch += (target - pitch) * glideK;

        // --- cutoff ---
        const float baseHz = 40.0f * std::exp2 (p.cutoff * 8.0f - p.envMod * 1.5f);
        const float octs   = p.envMod * 5.0f * meg + p.accent * 3.0f * accSweep;
        const float fc     = std::clamp (baseHz * std::exp2 (octs), 20.0f, std::min (18000.0f, fsOS * 0.12f));
        filter.setCutoff (fc);

        // --- oscillator + filter at the oversampled rate ---
        const float hz = 440.0f * std::exp2 ((pitch + p.tune - 69.0f) / 12.0f);
        const float dt = std::min (hz / fsOS, 0.45f);
        float os[kOversample];
        for (int i = 0; i < kOversample; ++i)
        {
            phase += dt;
            if (phase >= 1.0f)
                phase -= 1.0f;

            const float saw = 2.0f * phase - 1.0f - polyBlep (phase, dt);
            float half = phase + 0.5f;
            if (half >= 1.0f)
                half -= 1.0f;
            const float sq = (phase < 0.5f ? 1.0f : -1.0f) + polyBlep (phase, dt) - polyBlep (half, dt);

            const float x = saw + (0.85f * sq - saw) * p.wave;
            os[i] = filter.process (x * 0.8f);
        }
        float y = decimator.process (os);

        // --- VCA, drive, output coupling ---
        const float accentGain = 1.0f + p.accent * (accentNote ? 0.35f + 0.65f * meg : 0.0f);
        y *= amp * (0.35f + 0.65f * ampHold) * accentGain * (1.0f + 0.7f * p.reso);

        if (p.drive > 0.0f)
            y = fastTanh (y * driveGain) / std::sqrt (driveGain);

        outHpState += outHpK * (y - outHpState);
        return 2.0f * (y - outHpState);
    }

    DiodeLadder& getFilter() { return filter; }

private:
    float coefForTau (float seconds) const { return 1.0f - std::exp (-1.0f / (seconds * fs)); }

    VoiceParams params;
    DiodeLadder filter;
    Decimator   decimator;

    float fs = 48000.0f, fsOS = 192000.0f;
    float phase = 0.0f, pitch = 48.0f, target = 48.0f;
    float meg = 0, amp = 0, ampHold = 0, accCap = 0, outHpState = 0;
    float attackK = 0, releaseK = 0, holdK = 0, megNormK = 0, megAccK = 0;
    float glideK = 0, accChargeK = 0, accDisK = 0, outHpK = 0, driveGain = 1;
    bool gate = false, sliding = false, accentNote = false;
};

//==============================================================================
struct Step
{
    int  note   = 0;    // 0..12 semitones above root
    int  octave = 0;    // -1, 0, +1
    bool gate   = true;
    bool accent = false;
    bool slide  = false;
};

constexpr int kMaxSteps = 16;

// Host-synced 16th-note step sequencer. Call tick() once per sample.
class StepSequencer
{
public:
    void reset() { lastStepAbs = -1; gated = false; prevSlide = false; currentStep = -1; }

    // ppq = quarter-note position of this sample. Returns true if it changed the voice.
    void tick (AcidVoice& voice, bool playing, double ppq, const Step* steps, int numSteps,
               int baseNote, float swing, float gateLength = 0.5f)
    {
        if (! playing)
        {
            if (lastStepAbs >= 0 && gated)
                voice.noteOff();
            reset();
            return;
        }

        // Pairs of 16ths; the second of each pair is pushed late by swing.
        const double pos      = std::max (0.0, ppq) * 4.0;
        const double pairIdx  = std::floor (pos * 0.5);
        const double within   = pos - pairIdx * 2.0;
        const double swingPt  = 1.0 + std::clamp ((double) swing, 0.0, 1.0) * 0.5;
        const bool   odd      = within >= swingPt;
        const double frac     = odd ? (within - swingPt) / (2.0 - swingPt) : within / swingPt;
        const long long stepAbs = (long long) pairIdx * 2 + (odd ? 1 : 0);

        numSteps = std::clamp (numSteps, 1, kMaxSteps);

        if (stepAbs != lastStepAbs)
        {
            const bool contiguous = stepAbs == lastStepAbs + 1;
            lastStepAbs = stepAbs;
            currentStep = (int) (stepAbs % numSteps);
            const Step& s = steps[currentStep];

            if (s.gate)
            {
                const bool doSlide = contiguous && gated && prevSlide;
                voice.noteOn (baseNote + s.note + 12 * s.octave, s.accent, doSlide);
                gated     = true;
                prevSlide = s.slide;
            }
            else
            {
                if (gated)
                    voice.noteOff();
                gated = false;
                prevSlide = false;
            }
        }

        if (gated && ! prevSlide && frac >= gateLength)
        {
            voice.noteOff();
            gated = false;
        }
    }

    int getCurrentStep() const { return currentStep; }

private:
    long long lastStepAbs = -1;
    bool gated = false, prevSlide = false;
    int currentStep = -1;
};

} // namespace squelch
