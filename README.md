<p align="center">
  <img src="Source/Assets/icon.png" alt="Modular Voice Studio Logo" width="120" height="120" style="border-radius: 24px; box-shadow: 0 10px 30px rgba(0, 180, 216, 0.35);">
</p>

<h1 align="center">Modular Voice Studio</h1>

<p align="center">
  <strong>Next-Generation Real-Time Modular Vocal Processing Suite & 3D Spatial Audio DSP Engine</strong>
</p>

<p align="center">
  <a href="https://github.com/rootcf"><img src="https://img.shields.io/badge/Developer-Furkan%20%22rootcf%22%20%C3%87entek-00b4d8?style=for-the-badge&logo=github" alt="Developer"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg?style=for-the-badge" alt="License: GPL 3.0"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/JUCE-8.0-orange?style=for-the-badge" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Formats-VST3%20%7C%20VST2%20%7C%20Standalone-purple?style=for-the-badge" alt="Plugin Formats">
  <img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D6?style=for-the-badge&logo=windows" alt="Windows x64">
</p>

---

## 🌟 Overview

**Modular Voice Studio** is an open-source, ultra-low-latency, node-based digital signal processing (DSP) environment engineered specifically for **voiceover artists, streamers, podcasters, broadcast engineers, content creators, and audio purists**.

Built from the ground up with **C++20** and the **JUCE 8** framework, Modular Voice Studio combines the visual flexibility of a modular synth rack with studio-grade vocal processing. Build custom audio routing chains in real time with zero phase artifacts, deep-learning noise suppression, surgical spectral correction, and immersive 3D binaural positioning.

---

## 🚀 Key Features

* 🎛️ **Modular Node-Based DSP Canvas**: Freely place, connect, re-route, and inspect unlimited audio modules with dynamic cable physics, real-time signal glow, and smooth pan/zoom canvas controls.
* 🧠 **AI Neural Noise Suppression**: Integrated **RNNoise** recurrent neural network (RNN) for real-time background fan, AC, and room hum removal without phase comb filtering or robotic speech degradation.
* 🎧 **Interactive 3D Binaural Spatial Realm**: Realistic 3D Cartesian room acoustic simulation with listener head-rotation modeling, multi-band frequency splitting, distance attenuation, and reverberant boundary absorption.
* 🎙️ **Transparent Dynamic De-Esser**: Multi-band spectral-ratio sibilance detector that surgically attenuates harsh consonants (`s`, `sh`, `ch`, `z`) while preserving full vocal air and presence.
* 📊 **Surgical Equalization & Analysis**: 7-Band Parametric EQ with Real-Time Spectrum Analyzer (RTA) and musical 3-Band Channel EQ with interactive curve manipulation.
* 🔀 **Linkwitz-Riley 4th Order Crossover**: Split stereo audio into discrete LOW, MID, and HIGH bands (24 dB/oct) for multi-band dynamics and re-combine with zero phase distortion.
* 📦 **Container Rack System**: Group complex sub-chains into single draggable container modules for clean canvas hierarchy.
* 🔄 **Bidirectional DAW Preset Sync**: Full host program synchronization supporting instant patch switching from DAW track headers and automation lanes.
* ⚡ **Triple-Format Distribution**: Runs seamlessly as a **64-bit VST3 plugin**, **VST2 plugin**, or standalone desktop application with ASIO / DirectSound low-latency drivers.

---

## 🎛️ Comprehensive Module Catalog

### 1. 🌐 Spatial & Tone Shaping
| Module | Description | Key Capabilities |
| :--- | :--- | :--- |
| **Spatial 3D** | Binaural 3D Acoustic Realm | 3D room simulation, listener head tracking, multi-band spatial panning (Low/Mid/High), wall resonance absorption |
| **Proximity** | Acoustic Distance & Body | Inverse-square proximity bass boost and high-frequency air dampening modeling |
| **Broadcast Morph** | Microphone Character Cloner | 6 broadcast microphone profiles (SM7B, U87, Electro-Voice RE20, C414, Ribbon, Telephone) |
| **Aural Exciter** | Harmonic Air & High-End Synthesizer | Non-linear even-harmonic generation for breathiness and silky vocal shine |
| **Phantom Sub** | Sub-Harmonic Fundamental Generator | Reconstructs chest resonance and fundamental sub-frequencies for deep broadcast warmth |
| **Vocal Doubler** | Psychoacoustic Stereo Widener | Multi-voice micro-detuning, stereo Haas widening, and humanized vocal thickening |
| **Saturation** | Analog Tube & Tape Warmer | Symmetrical soft-clipping, triode tube harmonics, and asymmetric tape compression curves |
| **Phase Rotator** | All-Pass Phase Symmetry Enhancer | Aligns voice phase asymmetry in male and female spoken dialog for maximum headroom |

### 2. 🎚️ Spectral & Frequency Processing
| Module | Description | Key Capabilities |
| :--- | :--- | :--- |
| **De-Esser** | Spectral-Ratio Dynamic De-Esser | 4 target frequency bands (Low-Hi, Mid-Hi, High, Hi-End), Q sharpness control, delta audition mode |
| **Parametric EQ** | 7-Band Precision RTA Equalizer | Low Cut, Low Shelf, 3 Parametric Bell Filters, High Shelf, High Cut with interactive curve dragging |
| **3-Band EQ** | Musical 3-Band Channel Strip | Low Shelf (100Hz), Sweepable Mid Bell (200Hz - 8kHz), High Shelf (10kHz) |
| **Dynamic EQ** | Frequency-Specific Dynamic Tamer | Dynamic threshold-dependent resonance suppression for problematic voice spikes |
| **Spectral Clarity** | Resonance & Harshness Suppressor | Multi-band dynamic smoothing for boxy and nasal vocal frequencies |
| **Crossover Splitter**| 3-Way Linkwitz-Riley LR4 Splitter | Splits 1 stereo audio signal into Low, Mid, and High frequency bands (24 dB/oct) |
| **Crossover Joiner**  | 3-Way Modular Combiner | Phase-aligned 3-to-1 band summing mixer with independent gain trims, solo, and mute |

### 3. 🧹 Cleanup & Audio Restoration
| Module | Description | Key Capabilities |
| :--- | :--- | :--- |
| **Noise Suppression**| Deep Learning RNNoise AI | Real-time neural network background noise, hum, and hiss eliminator |
| **De-Breath** | Automatic Inhalation Suppressor | Envelope-tracking quiet breath and gasp reduction |
| **De-Plosive** | Mic Pop & Low-End Thump Filter | High-pass transient limiter targeting plosives (`p`, `b`, `t`) |
| **De-Click** | Mouth Saliva & Click Tamer | Micro-transient spike detector and smooth cubic interpolator |
| **De-Reverb** | Room Ambience & Echo Removal | Spectral subtraction for wet, untreated recording spaces |
| **AEC** | Acoustic Echo Cancellation | Double-talk detector and speaker-to-mic feedback suppressor |

### 4. ⚡ Dynamics & Utilities
| Module | Description | Key Capabilities |
| :--- | :--- | :--- |
| **Compressor** | VCA / Optical / FET Dynamic Leveller | Peak/RMS detection, soft knee, auto make-up gain, sidechain filter |
| **Gate / Expander** | Ultra-Fast Noise Gate | Lookahead hysteresis gating, hold/release curves |
| **Limiter** | True Peak Brickwall Mastering Limiter | Inter-sample peak protection, zero overs |
| **Gain / Trim** | Channel Level & Phase Inverter | Precise ±24 dB digital trim, polarity reverse, mute |
| **Oscilloscope** | Real-Time Vector Waveform Scope | Synchronized trigger modes, freeze frame, live visual feedback |

---

## 🏛️ System Architecture

```
                 ┌─────────────────────────────────────────┐
                 │       DAW Track / Microphone (ASIO)     │
                 └────────────────────┬────────────────────┘
                                      │
                                      ▼
                 ┌─────────────────────────────────────────┐
                 │     Modular Voice Studio DSP Engine     │
                 │ ┌─────────────────────────────────────┐ │
                 │ │        Real-Time Modular Graph      │ │
                 │ │                                     │ │
                 │ │  [IN] ──► [RNNoise AI] ──► [EQ]     │ │
                 │ │                  │                  │ │
                 │ │                  ▼                  │ │
                 │ │             [De-Esser]              │ │
                 │ │                  │                  │ │
                 │ │                  ▼                  │ │
                 │ │          [3D Spatial Realm]         │ │
                 │ │                  │                  │ │
                 │ │                  ▼                  │ │
                 │ │               [OUT]                 │ │
                 │ └─────────────────────────────────────┘ │
                 └────────────────────┬────────────────────┘
                                      │
                                      ▼
                 ┌─────────────────────────────────────────┐
                 │        Master Output & RTA Monitor      │
                 └─────────────────────────────────────────┘
```

---

## 💻 Tech Stack

* **Language**: C++20 standard
* **Framework**: [JUCE 8](https://juce.com/) (Audio Processors, ValueTree, OpenGL Canvas, DirectWrite)
* **DSP & Neural Networks**: [RNNoise](https://github.com/xiph/rnnoise) (Xiph.Org Foundation)
* **Typography**: [Inter Font](https://rsms.me/inter/) by Rasmus Andersson (SIL Open Font License 1.1)
* **Build System**: CMake 3.22+ & Microsoft Visual Studio MSBuild

---

## 🔧 Building from Source

### Prerequisites
1. **Windows 10 / 11 (64-bit)**
2. **Visual Studio 2022 or Visual Studio 2026** (with *Desktop development with C++*)
3. **CMake 3.22+**
4. **Git**

### Clone & Build

```bash
# 1. Clone the repository recursively (includes JUCE submodule)
git clone --recursive https://github.com/rootcf/ModularVoiceStudio.git
cd ModularVoiceStudio

# 2. Configure with CMake
cmake -B build -G "Visual Studio 18 2026" -A x64

# 3. Build All Targets (Standalone, VST3, VST2) in Release Mode
cmake --build build --config Release --parallel
```

### Compiled Binary Output Paths:
* **Standalone Application**: `build/ModularVoiceStudio_artefacts/Release/Standalone/Modular Voice Studio.exe`
* **VST3 Plugin**: `build/ModularVoiceStudio_artefacts/Release/VST3/Modular Voice Studio.vst3`
* **VST2 Plugin**: `build/ModularVoiceStudio_artefacts/Release/VST/Modular Voice Studio.dll`

---

## 💾 Patch Format (`.mvs`)

Modular Voice Studio uses a portable, human-readable JSON-based patch format (`.mvs`) storing complete graph topologies, node coordinates, parameter value trees (`APVTS`), and custom cable routes. Patches are interchangeable across all supported DAWs and the Standalone application.

---

## 🎚️ DAW Compatibility Matrix

Tested and fully supported across major 64-bit Digital Audio Workstations on Windows:

| Host / DAW | VST3 | VST2 | Standalone | Preset Sync |
| :--- | :---: | :---: | :---: | :---: |
| **FL Studio 21 / 2024** | ✅ | ✅ | — | ✅ |
| **Ableton Live 11 / 12** | ✅ | ✅ | — | ✅ |
| **Cockos REAPER 7** | ✅ | ✅ | — | ✅ |
| **Steinberg Cubase 12 / 13** | ✅ | ✅ | — | ✅ |
| **PreSonus Studio One 6** | ✅ | ✅ | — | ✅ |
| **OBS Studio (Filter)** | ✅ | ✅ | — | — |
| **Desktop / Live ASIO** | — | — | ✅ | ✅ |

---

## 👨‍💻 Author & Credits

* **Lead Architect & Developer**: **Furkan "rootcf" Çentek**
  * GitHub: [@rootcf](https://github.com/rootcf)

---

## 📄 License & Attributions

This project is licensed under the **GNU General Public License v3.0 (GPLv3)** - see the [LICENSE](LICENSE) file for details.

### Third-Party Libraries:
* **JUCE Framework**: Copyright (c) Raw Material Software / PACE Anti-Piracy Inc. - Licensed under GPLv3 / Commercial.
* **RNNoise**: Copyright (c) 2017 Mozilla, Xiph.Org Foundation, Jean-Marc Valin - Licensed under BSD 3-Clause.
* **Inter Font**: Copyright (c) 2016-2024 Rasmus Andersson - Licensed under SIL Open Font License 1.1.
