// MFR-303 - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
// Offline checks for the Squelch DSP: filter calibration + a rendered demo WAV.
#include "../Source/SquelchDSP.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

using namespace squelch;

static void writeWav (const std::string& path, const std::vector<float>& data, int sr)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) data.size() * 2;
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 2); u16 (2); u16 (16);
    f.write ("data", 4); u32 (bytes);
    for (float s : data)
    {
        const auto v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f);
        f.write ((const char*) &v, 2);
    }
}

// Ring the filter with an impulse; return the tail RMS and the ringing frequency.
static void ring (float fc, float k, float fsOS, float& tailRms, float& freq)
{
    DiodeLadder f;
    f.prepare (fsOS);
    f.setCutoff (fc);
    f.setFeedback (k);
    const int n = (int) fsOS;       // 1 s
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i)
        y[(size_t) i] = f.process (i < 8 ? 0.05f : 0.0f);

    double sum = 0.0;
    int start = n - n / 5, crossings = 0, first = -1, last = -1;
    for (int i = start; i < n; ++i)
    {
        sum += (double) y[(size_t) i] * y[(size_t) i];
        if (y[(size_t) i - 1] < 0.0f && y[(size_t) i] >= 0.0f)
        {
            if (first < 0) first = i;
            last = i;
            ++crossings;
        }
    }
    tailRms = (float) std::sqrt (sum / (n - start));
    freq = crossings > 1 ? (crossings - 1) * fsOS / (float) (last - first) : 0.0f;
}

int main()
{
    const float fsOS = 48000.0f * kOversample;

    std::printf ("fc(Hz)   k_threshold   ring_freq(Hz)   ratio\n");
    for (float fc : { 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f })
    {
        float lo = 0.0f, hi = 60.0f, rms = 0, fr = 0;
        for (int it = 0; it < 30; ++it)
        {
            const float mid = 0.5f * (lo + hi);
            ring (fc, mid, fsOS, rms, fr);
            (rms > 1.0e-4f ? hi : lo) = mid;
        }
        ring (fc, hi * 1.02f, fsOS, rms, fr);
        std::printf ("%7.0f   %10.3f   %12.1f   %6.3f\n", fc, hi, fr, fr / fc);
    }

    // Demo: 8 bars of the default pattern at 128 BPM, with a slow cutoff sweep.
    const Step pattern[kMaxSteps] = {
        { 0, 0, true, true, false },  { 0, 0, true, false, false }, { 0, 1, true, false, true },
        { 3, 0, true, false, false }, { 0, 0, false, false, false }, { 0, 0, true, true, false },
        { 7, 0, true, false, true },  { 10, 0, true, false, false }, { 0, 0, true, false, false },
        { 0, 1, true, true, false },  { 0, 0, false, false, false }, { 5, 0, true, false, true },
        { 7, 0, true, false, false }, { 0, 0, true, true, false },  { 3, 1, true, false, true },
        { 0, 0, true, false, false },
    };

    const int sr = 48000;
    const double bpm = 128.0;
    const int total = (int) (sr * 60.0 / bpm * 4 * 8);

    AcidVoice voice;
    voice.prepare (sr);
    StepSequencer seq;
    VoiceParams p;

    std::vector<float> out ((size_t) total);
    float peak = 0.0f;
    bool bad = false;
    for (int i = 0; i < total; ++i)
    {
        if (i % 64 == 0)
        {
            const float t = (float) i / total;
            p.cutoff = 0.15f + 0.45f * (0.5f - 0.5f * std::cos (t * 2.0f * kPi));
            p.reso   = 0.85f;
            p.envMod = 0.6f;
            p.decay  = 0.35f;
            p.accent = 0.8f;
            p.drive  = t > 0.5f ? 0.35f : 0.0f;
            voice.setParams (p);
        }
        seq.tick (voice, true, i * bpm / 60.0 / sr, pattern, 16, 36, 0.0f);
        const float s = voice.renderSample() * 0.5f;   // -6 dB volume default
        if (! std::isfinite (s)) bad = true;
        peak = std::max (peak, std::abs (s));
        out[(size_t) i] = s;
    }
    writeWav ("squelch_demo.wav", out, sr);
    std::printf ("\ndemo: %d samples, peak %.3f (%.1f dBFS), %s\n", total, peak,
                 20.0f * std::log10 (peak + 1e-9f), bad ? "NON-FINITE SAMPLES!" : "all finite");
    return bad ? 1 : 0;
}
