# Obstacle Space Echo

> Multi-head tape delay & spring reverb VST3/AU plugin — inspired by the Roland RE-201.

![Platform](https://img.shields.io/badge/platform-macOS-lightgrey)
![Format](https://img.shields.io/badge/format-VST3%20%7C%20AU%20%7C%20Standalone-orange)
![License](https://img.shields.io/badge/license-MIT-blue)
![JUCE](https://img.shields.io/badge/JUCE-8.0.12-green)

---

## Features

- **3 virtual tape heads** with authentic RE-201 spacing ratios
- **12 echo modes** — single, double, triple heads with optional spring reverb
- **Wow & flutter** — dual LFO pitch modulation for tape instability
- **Tape saturation** — soft `tanh` clipping on the record head
- **Spring reverb** — Schroeder reverberator tuned for spring character
- **Bass / Treble EQ** — inside the feedback loop (accumulates per repeat)
- **Industrial UI** — copper/charcoal panel, LED VU meters, 12-position mode selector
- **Built-in test tone** — test without a DAW

---

## Mode Reference

| Mode | Heads active | Reverb |
|------|-------------|--------|
| 1    | H1          | —      |
| 2    | H2          | —      |
| 3    | H3          | —      |
| 4    | H1 + H2     | —      |
| 5    | H1 + H3     | —      |
| 6    | H2 + H3     | —      |
| 7    | H1 + H2 + H3| —      |
| 8    | H1          | ✓      |
| 9    | H2          | ✓      |
| 10   | H3          | ✓      |
| 11   | H1 + H2 + H3| ✓      |
| 12   | —           | ✓ only |

---

## Parameters

| Knob        | Range         | Description                              |
|-------------|---------------|------------------------------------------|
| INPUT       | 0 – 100%      | Input gain                               |
| RATE        | 20 – 500 ms   | Tape speed / base delay time             |
| INTENSITY   | 0 – 95%       | Feedback amount                          |
| BASS        | ±12 dB        | Low-shelf EQ (in feedback loop)          |
| TREBLE      | ±12 dB        | High-shelf EQ (in feedback loop)         |
| WOW/FLT     | 0 – 100%      | Wow (0.4 Hz) + flutter (8 / 13.7 Hz)    |
| SATURATION  | 0 – 100%      | Tape saturation drive                    |
| ECHO LVL    | 0 – 100%      | Echo wet level                           |
| REVB LVL    | 0 – 100%      | Spring reverb level (active in modes 8-12)|

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

Open the **Standalone** app from the build folder, then click the **TEST** button in the top-right corner of the interface. It generates a chord (A/C#) every 1.5 seconds so you can hear the echo without any external audio source.

---

## License

MIT — see [LICENSE](LICENSE)

---

*Built with [JUCE](https://juce.com)*
