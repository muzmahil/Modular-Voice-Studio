<p align="center">
  <img src="Standalone/Assets/icon.png" alt="Modular Voice Studio Logo" width="120" height="120" style="border-radius: 24px; box-shadow: 0 10px 30px rgba(0, 180, 216, 0.35);">
</p>

<h1 align="center">Modular Voice Studio</h1>

<p align="center">
  <strong>Next-Generation Real-Time Modular Vocal Processing Suite, Multi-Channel Hardware Mixer & 3D Spatial Audio Engine</strong>
</p>

<p align="center">
  <a href="https://github.com/muzmahil"><img src="https://img.shields.io/badge/Developer-muzmahil-00b4d8?style=for-the-badge&logo=github" alt="Developer"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg?style=for-the-badge" alt="License: GPL 3.0"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/.NET-8.0-512BD4?style=for-the-badge&logo=dotnet" alt=".NET 8">
  <img src="https://img.shields.io/badge/Avalonia-11-purple?style=for-the-badge" alt="Avalonia UI">
  <img src="https://img.shields.io/badge/JUCE-8.0-orange?style=for-the-badge" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Formats-Standalone%20%7C%20VST3%20%7C%20VST2-success?style=for-the-badge" alt="Formats">
  <img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D6?style=for-the-badge&logo=windows" alt="Windows x64">
</p>

---

## 🌟 Overview

**Modular Voice Studio** is an open-source, ultra-low-latency, multi-channel vocal processing and audio routing suite engineered specifically for **streamers, voiceover artists, podcasters, broadcast engineers, content creators, and audio purists**.

Powered by a high-performance **C++20 / JUCE 8 DSP Core** and a modern **.NET 8 / Avalonia 11 desktop GUI**, Modular Voice Studio combines:
1. **A Modular Node-Based DSP Canvas** with 25+ studio modules.
2. **A Hardware Channel Strip Mixer** for direct mic, virtual VB-Cable output, and zero-latency headphone monitoring.
3. **A Dynamic Serial Effect Rack** with live VST3 plugin hosting and creative vocal transformations.

---

## 🚀 Key Features

* 🎛️ **Dual-Architecture Desktop Application**:
  * **C# / Avalonia 11 Frontend**: Ultra-responsive, GPU-accelerated hardware mixer UI styled with neutral dark studio aesthetics.
  * **C++20 Hybrid DSP Core (`ModularVoiceStudio_Core.dll`)**: Real-time thread-safe audio engine with double-precision floating-point calculation.
* 🎙️ **Live Multi-Bus Channel Routing**:
  * **Mic Input**: Zero-latency hardware microphone capture.
  * **VB-Cable Output (Bus B1)**: Streams processed broadcast voice directly to Discord, OBS Studio, Zoom, Skype, and games.
  * **Headphone Monitor (Bus A1)**: Dedicated low-latency headphone monitoring with zero double-voice/comb-filter artifacts.
* ⚡ **Ultra-Low Latency Engine**:
  * Optimized buffer sizing (128 / 256 samples, ~2.6 ms).
  * Real-time granular pitch shifting with sub-7ms lookback windows and zero-shift bypass.
* 📦 **Dynamic Serial Effect Rack**:
  * Real-time serial insert processing with smooth Wet/Dry blend faders (lowering fader dials down effect intensity without muting the dry voice).
  * Fast bypass, solo, mute, and live stereo VU metering per strip.
* 🔌 **Universal 64-Bit VST3 Plugin Hosting**:
  * Automated system-wide VST3 directory scanner (`C:\Program Files\Common Files\VST3`, `VSTPlugins`, etc.).
  * Direct file loading and native GUI window hosting for third-party plugins (Valhalla, FabFilter, iZotope, etc.).
* 🧠 **Studio Modular Canvas**:
  * Full node graph editor with 25+ built-in DSP processors (7-Band Parametric EQ, LR4 Crossover, Dynamic De-Esser, 3D Spatial Realm, Broadcast Mic Modeling, Compressor, OTT, Limiter, etc.).
* 🔤 **Clean Embedded Typography**:
  * Embedded **Inter** font family with Helvetica fallbacks for razor-sharp rendering on all high-DPI screens.

---

## 🎛️ Effect Rack & Creative Vocal FX

| Effect Module | Description | Key Features |
| :--- | :--- | :--- |
| **Pitch Shifter** | Studio Vocal Pitch & Formant Shifter | Octave shift (±24 st), Fine tune (±100 ct), Formant timbre/gender shift (±12 st), ultra-low latency grain tracking |
| **Studio Reverb** | Acoustic Space Simulator | Room size, high-frequency damping, stereo width, wet/dry blend |
| **Echo Delay** | Stereo Ping-Pong & Feedback Delay | Millisecond sync, feedback resonance, stereo cross-channel ping-pong |
| **Robot Voice** | Sci-Fi Ring Modulator & Bitcrusher | Modulation rate, depth, sample-rate crushing, lo-fi bit reduction |
| **Vocal Doubler** | Psychoacoustic Stereo Chorus | Micro-pitch detune, stereo Haas spatial widening, vocal thickening |
| **Analog Saturation** | Tube & Tape Harmonic Warmer | Soft-clipping drive, odd/even tube warmth, asymmetrical tape curves |
| **VST3 Host** | 64-Bit External Plugin Rack | Hosts any external VST3 audio effect with full editor window and parameter automation |

---

## 🏛️ System Architecture

```
                       ┌──────────────────────────────────────────────┐
                       │           Hardware Microphone Input          │
                       └──────────────────────┬───────────────────────┘
                                              │
                                              ▼
                       ┌──────────────────────────────────────────────┐
                       │     Modular Voice Studio (C++ Core DSP)      │
                       │ ┌──────────────────────────────────────────┐ │
                       │ │   Modular DSP Canvas / Pre-Processing    │ │
                       │ └────────────────────┬─────────────────────┘ │
                       │                      │                       │
                       │                      ▼                       │
                       │ ┌──────────────────────────────────────────┐ │
                       │ │        Dynamic Serial Effect Rack        │ │
                       │ │  [Pitch Shift] ➔ [Reverb] ➔ [VST3 FX]   │ │
                       │ └────────────────────┬─────────────────────┘ │
                       └──────────────────────┼───────────────────────┘
                                              │
                       ┌──────────────────────┴───────────────────────┐
                       │                                              │
                       ▼                                              ▼
        ┌─────────────────────────────┐                ┌─────────────────────────────┐
        │  Bus B1: VB-Cable Output    │                │  Bus A1: Headphone Monitor  │
        │  (Discord, OBS, Games)      │                │  (Zero-Latency Live Audio)  │
        └─────────────────────────────┘                └─────────────────────────────┘
```

---

## 💻 Tech Stack

* **Core Audio & DSP**: C++20 standard, [JUCE 8](https://juce.com/) framework (AudioDeviceManager, APVTS, VST3 Hosting).
* **Desktop Application Frontend**: C# / [.NET 8.0](https://dotnet.microsoft.com/), [Avalonia UI 11.2](https://avaloniaui.net/) (Cross-platform XAML vector GUI).
* **Interoperability Layer**: High-speed C-ABI Native Export (`MVS_Core_Export.cpp` / `NativeBridge.cs`).
* **Typography**: [Inter Font](https://rsms.me/inter/) (SIL Open Font License 1.1) with Helvetica fallback.
* **Build System**: CMake 3.22+, MSBuild / Visual Studio 2022+, `dotnet` CLI.

---

## 🔧 Building from Source

### Prerequisites
1. **Windows 10 / 11 (64-bit)**
2. **Visual Studio 2022 or Visual Studio 2026** (with *Desktop development with C++*)
3. **.NET 8.0 SDK**
4. **CMake 3.22+**
5. **Git**

### Step 1: Clone Repository

```bash
git clone --recursive https://github.com/muzmahil/Modular-Voice-Studio.git
cd Modular-Voice-Studio
```

### Step 2: Build C++ Core Engine & Plugins

```bash
# Configure CMake
cmake -B build -G "Visual Studio 18 2026" -A x64

# Build ModularVoiceStudio_Core DLL and VST targets
cmake --build build --config Release --parallel
```

### Step 3: Build Standalone C# Application

```bash
# Copy Core DLL to Standalone output directory
copy build\Release\ModularVoiceStudio_Core.dll Standalone\bin\Release\net8.0-windows\

# Build Standalone App
dotnet build Standalone/ModularVoiceStudio.App.csproj -c Release
```

### Step 4: Run Application

```bash
.\Standalone\bin\Release\net8.0-windows\ModularVoiceStudio.exe
```

---

## 📦 Building Installer Package

To generate a standalone setup installer executable:

```powershell
# Run the automated packaging script
.\Build-Installer.ps1
```

The output setup bundle will be generated in `dist/ModularVoiceStudio_Setup.exe`.

---

## 📄 License & Attributions

This project is licensed under the **GNU General Public License v3.0 (GPLv3)** - see the [LICENSE](LICENSE) file for details.

### Third-Party Libraries:
* **JUCE Framework**: Copyright (c) Raw Material Software / PACE Anti-Piracy Inc. - Licensed under GPLv3 / Commercial.
* **Avalonia UI**: Copyright (c) AvaloniaUI OÜ - Licensed under MIT.
* **RNNoise**: Copyright (c) 2017 Mozilla, Xiph.Org Foundation, Jean-Marc Valin - Licensed under BSD 3-Clause.
* **Inter Font**: Copyright (c) 2016-2024 Rasmus Andersson - Licensed under SIL Open Font License 1.1.
