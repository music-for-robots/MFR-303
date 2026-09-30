# MFR-303

[![Build](https://github.com/music-for-robots/MFR-303/actions/workflows/build.yml/badge.svg)](https://github.com/music-for-robots/MFR-303/actions/workflows/build.yml)

A free, open-source acid bass synthesizer in the spirit of the classic 303, by [Music For Robots](https://musicforrobots.com).

MFR-303 is a monophonic bass-line synth. It has a resonant diode-ladder style filter, the accent and slide behaviour that makes acid lines squelch, and a built-in 16-step sequencer that syncs to your DAW.

## Features

- **Sound**
  - Saw and square oscillator that can blend between the two.
  - Four-pole filter modelled on the diode ladder, run at 4× oversampling. It has a saturating input and a highpass in its feedback path, so turning up resonance thins out the low end.
  - Filter envelope with Env Mod and Decay controls.
  - Accent. Consecutive accents build on each other, giving the classic "wow".
  - Slide (glide) with adjustable time.
  - Drive stage.
- **Two ways to play**
  - **MIDI mode:** play it from your piano roll. Notes with velocity 100 or higher are accented, and overlapping notes slide.
  - **Sequencer mode:** a 16-step pattern with note, octave up/down, gate, accent and slide on every step. It also has swing, gate length, a root note and a variable pattern length, plus random, clear and shift buttons. Holding a MIDI key transposes the pattern.
- **Knob handling:** double-click or Alt-click resets a knob to its default. Ctrl- or Shift-drag gives fine control. You can also click a value and type one in.

Formats: VST3 and Standalone (Windows 64-bit).

## Installing

Download the latest release from the [Releases](../../releases) page. Copy `MFR-303.vst3` into `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW.

## Building from source

Requirements: CMake 3.22+ and Visual Studio 2022 with the C++ desktop workload. JUCE 8 is included as a git submodule.

```
git clone --recursive https://github.com/music-for-robots/MFR-303.git
cd MFR-303
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built plugin ends up in `build/Squelch_artefacts/Release/VST3/`. ("Squelch" is the project's internal working name.)

`SquelchTest` is a small offline tool that checks the filter calibration and renders a demo WAV. It doesn't need JUCE.

To package it locally, run `scripts/package.ps1`. It needs [Inno Setup 6](https://jrsoftware.org/isinfo.php) and writes a zip and an installer to `dist/`.

## Releasing

1. Bump `VERSION` in `project(...)` in `CMakeLists.txt`.
2. Add a section for the new version to `CHANGELOG.md`.
3. Commit, then tag and push, e.g. `git tag v1.0.1 && git push origin v1.0.1`.

GitHub Actions then builds the plugin, runs the DSP test and [pluginval](https://github.com/Tracktion/pluginval) (strictness 10), and publishes a GitHub Release with the zip and the installer.

## Source layout

| Path | What's in it |
| --- | --- |
| `Source/SquelchDSP.h` | Oscillator, filter, envelopes, accent/slide voice and step sequencer (plain C++) |
| `Source/PluginProcessor.*` | Parameters, MIDI and sequencer handling, state save/load |
| `Source/PluginEditor.*` | User interface |
| `tools/SquelchTest.cpp` | Offline DSP test |
| `installer/`, `scripts/package.ps1` | Installer script and packaging |
| `.github/workflows/build.yml` | CI build, validation and releases |

## License

MFR-303 is licensed under the [GNU AGPLv3](LICENSE) (or any later version). It is built with [JUCE](https://juce.com), which is used under the AGPLv3, and with the Steinberg VST3 SDK.

VST is a trademark of Steinberg Media Technologies GmbH. TB-303 is a trademark of Roland Corporation. MFR-303 is an independent project and is not affiliated with or endorsed by Roland, Steinberg, or any other company.
