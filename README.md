# Obstacle Space Echo

> Multi-head tape delay & spring reverb VST3/AU plugin — inspired by the Roland RE-201.

![Platform](https://img.shields.io/badge/platform-macOS-lightgrey)
![Format](https://img.shields.io/badge/format-VST3%20%7C%20AU%20%7C%20Standalone-orange)
![License](https://img.shields.io/badge/license-MIT-blue)
![JUCE](https://img.shields.io/badge/JUCE-8.0.12-green)
![Version](https://img.shields.io/badge/version-1.3.0-brightgreen)

---

## Features

### Tape delay engine
- **3 virtual tape heads** with authentic RE-201 spacing ratios (×1.0, ×1.475, ×2.625)
- **12 echo modes** — single, double, triple heads with optional spring reverb
- **Wow & flutter** — dual LFO pitch modulation (0.4 Hz wow + 8 Hz / 13.7 Hz flutter)
- **Tape saturation** — soft `tanh` clipping on the record head
- **Bass / Treble EQ** — inside the feedback loop (accumulates per repeat)
- **Tape noise** — filtered hiss (200–8 kHz bandpass), adds vintage character

### Spring reverb
- **Schroeder reverberator** tuned for spring character (8 combs + 4 allpass)
- **Shimmer reverb** — granular pitch shifter (+1 octave) feeding back into the reverb tail,
  creating an endless rising harmonic shimmer (Valhalla-style algorithm)

### Performance features
- **FREEZE** — stops the tape write head, the buffer loops infinitely (infinite sustain pad)
- **PING-PONG stereo** — cross-feeds L/R feedback so echoes bounce left↔right
- **Soft limiter** — transparent tanh output stage prevents digital clipping at any setting
- **Per-sample parameter smoothing** — no zipper noise when turning knobs live

### Interface (960 × 460 px) — Roland RE-201 faithful
- **Three-section skeuomorphic panel** — dark aluminium left, centre, military-green right
- **Analog needle VU meter** — semi-circular arc, −20..+3 VU scale, 300 ms ballistics, peak hold
- **Rotary mode selector** — large chrome dial, 300° arc, 12 positions, click or drag
- **Brushed-aluminium texture** — fine horizontal lines on metal panels, hammertone finish on green panel
- **Chrome Phillips screws** — specular highlight, recessed head, directional gradient (8 total)
- **Recessed groove seams** — physical joint illusion between panel sections
- **Animated tape reels** — rotating at tape speed, freeze-aware, in footer strip
- **Oscilloscope** — CRT phosphor-green real-time waveform
- **Built-in test tone** — A/C# chord every 1.5 s, no external audio needed

---

## Mode Reference

| Mode | Heads active  | Reverb |
|------|--------------|--------|
| 1    | H1           | —      |
| 2    | H2           | —      |
| 3    | H3           | —      |
| 4    | H1 + H2      | —      |
| 5    | H1 + H3      | —      |
| 6    | H2 + H3      | —      |
| 7    | H1 + H2 + H3 | —      |
| 8    | H1           | ✓      |
| 9    | H2           | ✓      |
| 10   | H3           | ✓      |
| 11   | H1 + H2 + H3 | ✓      |
| 12   | —            | ✓ only |

---

## Parameters

| Control       | Range         | Description                                              |
|---------------|---------------|----------------------------------------------------------|
| INPUT         | 0 – 100%      | Input gain                                               |
| RATE          | 20 – 500 ms   | Tape speed / base delay time                             |
| INTENSITY     | 0 – 95%       | Feedback amount                                          |
| BASS          | ±12 dB        | Low-shelf EQ (in feedback loop — accumulates per repeat) |
| TREBLE        | ±12 dB        | High-shelf EQ (in feedback loop)                         |
| WOW/FLT       | 0 – 100%      | Wow (0.4 Hz) + flutter (8 / 13.7 Hz) pitch modulation   |
| SATURATE      | 0 – 100%      | Tape saturation drive (tanh soft clip)                   |
| ECHO LVL      | 0 – 100%      | Echo wet level                                           |
| REVERB LVL    | 0 – 100%      | Spring reverb level (active in modes 8–12)               |
| NOISE         | 0 – 100%      | Tape hiss level (200–8 kHz filtered noise)               |
| SHIMMER       | 0 – 100%      | Granular +1-octave shimmer on reverb tail                |
| **FREEZE**    | toggle        | Freezes tape loop — creates an infinite sustain pad      |
| **PING-PONG** | toggle        | Stereo cross-feed — echoes bounce left ↔ right           |

---

## Build from source

### Requirements

- macOS 10.13+
- Xcode Command Line Tools
- CMake ≥ 3.22

JUCE 8.0.12 is fetched automatically by CMake — no manual installation needed.

### Quick build

```bash
git clone https://github.com/laurentcbn/obstaclespaceecho.git
cd obstaclespaceecho
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

### Install

```bash
# VST3 (Ableton, Reaper, Cubase, Studio One…)
cp -r "build/SpaceEcho_artefacts/Release/VST3/Obstacle Space Echo.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/

# AU (Logic Pro, GarageBand)
cp -r "build/SpaceEcho_artefacts/Release/AU/Obstacle Space Echo.component" \
      ~/Library/Audio/Plug-Ins/Components/
```

After installing, rescan plugins in your DAW.

---

## Testing without a DAW

Open the **Standalone** app from the build folder. Click **TEST** in the top-right corner —
it generates an A/C# chord every 1.5 seconds so you can hear the effect without any
external audio source.

---

## DSP architecture

```
Input ──┬──────────────────────────────────────────────────────► dry
        │ × inputGain
        │
        │   ┌─── Tape Noise (filtered hiss) ─────────────────── inject
        │   │
        ▼   ▼
    ┌─────────────────┐
    │  TapeDelay (×2) │  3 heads · Catmull-Rom interp · wow/flutter LFOs
    │  FREEZE support │  tanh saturation · per-head LP/HP filter
    └────────┬────────┘
             │  active heads summed & normalised
             │
    ┌────────▼────────┐
    │  Bass/Treble EQ │  IIR shelving, updated only on change
    └────────┬────────┘
             │  ← feedbackL/R (one-sample delay, optional ping-pong cross-feed)
             │
    echo ────┼──────────────────────────────────────────────────► × echoLevel
             │
    ┌────────▼────────┐        ┌──────────────────────────────┐
    │  SpringReverb   │◄───────│  ShimmerChorus               │
    │  (modes 8–12)   │        │  Granular pitch shift +1 oct  │
    └────────┬────────┘        │  2 grains · Hanning window   │
             │                 └──────────────────────────────┘
    rev ─────┼──────────────────────────────────────────────────► × reverbLevel
             │
    ┌────────▼────────┐
    │   Soft Limiter  │  tanh(x·0.9)/0.9 — transparent below 0 dBFS
    └────────┬────────┘
             │
           Output (stereo)
```

---

## Changelog

### v1.3.0
- **Interface**: complete Roland RE-201 faithful skeuomorphic redesign (960 × 460 px)
- **Analog VU meter**: semi-circular needle, −20..+3 VU scale, 300 ms ballistics, peak hold
- **Rotary mode selector**: large chrome dial replacing button strip, 300° arc, click/drag
- **Panel textures**: brushed-aluminium lines on dark panels, hammertone finish on green panel
- **Skeuomorphic details**: chrome Phillips screws, recessed groove seams, silk-screened labels, corner vignette, outer frame bevel

### v1.2.0
- **Shimmer**: replaced fake chorus with proper granular +1-octave pitch shifter (Valhalla-style feedback architecture)
- **Parameter smoothing**: per-sample `SmoothedValue` (20 ms ramp) on all knobs — no more zipper noise
- **Soft limiter**: tanh output stage, transparent at normal levels

### v1.1.0
- New features: FREEZE, PING-PONG, TAPE NOISE, SHIMMER knobs
- New UI: 960×520 px, animated tape reels, CRT oscilloscope, FREEZE/PING-PONG buttons
- New DSP: TapeNoise (xorshift32 + bandpass filter), oscilloscope ring buffer

### v1.0.0
- Initial release: 3-head tape delay, 12 modes, spring reverb, wow/flutter, saturation, VU meters

---

## License

MIT — see [LICENSE](LICENSE)

---

*Built with [JUCE](https://juce.com)*
