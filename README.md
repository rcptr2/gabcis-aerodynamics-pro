# Gabci's AeroDynamics Pro

> Fluid-dynamics distortion built on a numerical solution of Burgers' equation.

[![Licence: AGPL v3](https://img.shields.io/badge/licence-AGPL--3.0-blue.svg)](LICENSE)
[![Build](https://github.com/rcptr2/gabcis-aerodynamics-pro/actions/workflows/build.yml/badge.svg)](https://github.com/rcptr2/gabcis-aerodynamics-pro/actions/workflows/build.yml)
![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20macOS%20Intel-lightgrey)
![Format](https://img.shields.io/badge/format-VST3%20%7C%20Standalone-green)
![Status](https://img.shields.io/badge/status-beta-orange)

🇭🇺 *A magyar leírás: [README.hu.md](README.hu.md)*

![Gabci's AeroDynamics Pro user interface](docs/images/aerodynamics-pro-ui.png)

*The fluid visualiser above the five engine controls, with the oversampling selector bottom right.*

Most saturation plug-ins shape a waveform with a static transfer curve. AeroDynamics Pro instead
treats the signal as a one-dimensional compressible flow and advances it through a numerical solution
of the viscous **Burgers equation** — the simplest partial differential equation that produces real
wave steepening and shock formation. The harmonics are a consequence of the physics, not of a
hand-drawn curve, so they move with the material.

Built with [JUCE](https://juce.com). The `processBlock` is allocation-free on the audio thread.

## How it works

The engine uses **operator splitting**, separating the two halves of the equation that want opposite
numerical treatment:

- **Linear transport (Flow Rate)** is handled by a Lagrange-interpolated `juce::dsp::DelayLine`
  running at the oversampled rate, up to 25 ms. Because it only reads an interpolated earlier sample,
  it introduces zero numerical diffusion — the earlier single-grid solution smeared the signal even
  with every effect control at zero.
- **The non-linear part** — `∂u/∂t + α·u·∂u/∂x = ν·∂²u/∂x²` — runs on a small fixed finite-difference
  grid, giving the wave steepening (Turbulence) and viscous damping (Viscosity) without the transport
  term's side effects.

Around that core:

- **Oversampling** — 4× or 8× equiripple FIR half-band chains, built ahead of time in
  `prepareToPlay()` and set to maximum quality. Aliasing destroys the organic character of the
  turbulence term, so this is not optional.
- **Stability** — the coefficient limits are derived from a coupled von Neumann analysis of the
  scheme's worst-case (Nyquist/checkerboard) mode rather than from separate CFL conditions.
- **DC blocker** and a latency-compensated dry path feeding a linear Mix crossfade.

## Parameters

| Parameter | Range | Default | Description |
|---|---|---|---|
| Pressure | 0 – 36 dB | 0 dB | Pre-gain into the engine. This is what drives the signal deep into the non-linear region — the most audible control by a wide margin. |
| Viscosity | 0 – 100 % | 30 % | Viscous damping term (ν). Smooths the flow. |
| Turbulence | 0 – 100 % | 30 % | Non-linear advection term (α). Produces the wave steepening. |
| Flow Rate | 0 – 100 % | 50 % | Transport delay, mapped to 0 – 25 ms at the oversampled rate. |
| Mix | 0 – 100 % | 100 % | Dry/wet crossfade against a latency-compensated dry path. |
| Oversampling | 4× / 8× | 8× | Anti-aliasing quality. |
| Bypass | on / off | off | Full bypass. |

## Status — beta

This is **version 0.6.1, a pre-release**. It is stable and its test suite passes, but the v0.6.0
architecture change (the operator split described above) left the Viscosity and Turbulence controls
subtler than intended at typical signal levels, and Flow Rate is now a pure delay rather than a
character control. Pressure remains the dominant parameter. Version 1.0.0 is held back pending a full
live validation pass; see [CHANGELOG.md](CHANGELOG.md).

If you want obvious movement from Viscosity and Turbulence, raise Pressure first.

## Installation

Pre-built binaries are on the
[Releases](https://github.com/rcptr2/gabcis-aerodynamics-pro/releases) page.

### Windows x64

1. Download `AeroDynamicsPro-vX.Y.Z-Windows-x64-VST3.zip`.
2. Unzip it and copy the `AeroDynamics Pro.vst3` folder into `C:\Program Files\Common Files\VST3\`.
3. Rescan plug-ins in your DAW.

### macOS (Intel)

The macOS binary is **x86_64 (Intel)**. It runs natively on Intel Macs and under Rosetta 2 in an
Intel-mode host on Apple Silicon; there is no arm64 slice.

1. Download `AeroDynamicsPro-vX.Y.Z-macOS-Intel-VST3.zip`.
2. Unzip it and copy `AeroDynamics Pro.vst3` into `/Library/Audio/Plug-Ins/VST3/`
   (or `~/Library/Audio/Plug-Ins/VST3/` for the current user only).
3. The build is not notarised, so clear the quarantine flag:
   ```bash
   xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/AeroDynamics Pro.vst3"
   ```
4. Rescan plug-ins in your DAW.

## Building from source

### Requirements

- CMake 3.24 or newer
- A C++20 compiler — **Visual Studio 2022** (Desktop development with C++) on Windows,
  **Xcode 15+** on macOS
- Git

JUCE 9.0.0 is pinned in `CMakeLists.txt` and downloaded automatically by CMake's `FetchContent` at
configure time. MinGW is not supported: JUCE rejects it explicitly, and its Windows backend needs
MSVC intrinsics and the Direct2D/DirectWrite headers.

> **The build directory path must not contain an apostrophe.** JUCE's generated VST3 `POST_BUILD`
> steps do not escape apostrophes in the shell command chains they emit. `CMakeLists.txt` checks for
> this and stops with a clear error rather than failing later. This is also why the bundle is named
> "AeroDynamics Pro" while the on-screen name is "Gabci's AeroDynamics".

### Windows

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DAERODYNAMICSPRO_BUILD_TESTS=OFF
cmake --build build --config Release --target AeroDynamicsProPlugin_VST3
```

### macOS

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64 -DAERODYNAMICSPRO_BUILD_TESTS=OFF
cmake --build build --config Release --target AeroDynamicsProPlugin_VST3
```

The finished bundle is written to
`build/AeroDynamicsProPlugin_artefacts/Release/VST3/AeroDynamics Pro.vst3`.

### Tests

The suite uses [Catch2](https://github.com/catchorg/Catch2), fetched automatically:

```bash
cmake -S . -B build -DAERODYNAMICSPRO_BUILD_TESTS=ON
cmake --build build --config Release --target AeroDynamicsProTests
```

Then run the resulting `AeroDynamicsProTests` executable from the build tree.

## Project layout

```
Source/          Plug-in source — processor, editor, fluid engine, oversampling manager,
                 DC blocker, visualiser, look and feel
Tests/           Catch2 unit tests
docs/            Design blueprint
CMakeLists.txt   Build definition; pins JUCE 9.0.0
CHANGELOG.md     Development history, phase by phase
```

## Tested with

- **macOS** (Intel, x86_64) — FL Studio 2026
- **Windows 11 x64** — FL Studio 2026

## Performance

![FL Studio plug-in performance monitor](docs/images/performance-monitor.png)

Measured in FL Studio 2026 on Windows 11 x64, on an ASUS ZenBook 13 with an Intel
Core i7-1065G7 — a low-power four-core laptop CPU, not a workstation. All seven
plug-ins ran simultaneously in the same project, with two stock Image-Line plug-ins
included for reference. The figures are FL Studio's own, captured with
*Reset on transport* enabled so that one-off initialisation spikes are excluded.

| Plug-in | CPU % | Time | Peak |
|---|---:|---:|---:|
| **Gabci's AeroDynamics Pro** | **17** | **251** | **353** |
| FLEX Bass *(Image-Line, reference)* | 9 | 125 | 275 |
| Gabci's MasterClear | 4 | 53 | 264 |
| Gabci's SmartMask Network *(instance 1)* | 3 | 43 | 554 |
| Gabci's PhaseLock Sub | 3 | 41 | 1306 |
| Emphasizer *(Image-Line, reference)* | 2 | 34 | 117 |
| Gabci's Acoustic Cloak | 2 | 36 | 191 |
| Gabci's MorphicPhaser | 2 | 27 | 152 |
| Gabci's SmartMask Network *(instance 2)* | 1 | 16 | 498 |
| Gabci's SpectralCarve Pro | 1 | 19 | 751 |

## Licence

Released under the **GNU Affero General Public License v3.0 or later** — see [LICENSE](LICENSE).

This choice is not arbitrary. AeroDynamics Pro is built with JUCE 9, which is dual-licensed under the
AGPLv3 and a commercial JUCE licence. Distributing a binary built from this source under the AGPLv3
branch is what makes it free to publish, and it obliges any derived work to be released under the
same terms with its source available.

## Attribution

- [JUCE](https://juce.com) — © Raw Material Software Limited, used here under the AGPLv3.
- [Catch2](https://github.com/catchorg/Catch2) — Boost Software License 1.0 (test builds only).
- VST® is a registered trademark of Steinberg Media Technologies GmbH. The VST 3 SDK bundled with
  JUCE is distributed by Steinberg under the MIT licence.

## Author

Gábor Tomori — *Gabci Audio*
