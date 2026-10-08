# Aura

**Aura** is a stereo/mono VST3 FFT spectral shifter by **355ERA**. It is designed as an insert effect for samples, field recordings, and instruments, with a soft botanical look suited to organic and botanica sound design.

![Aura interface preview](Aura-interface-preview.svg)

## Controls

- **Spectral** page — **Shift** continuously translates spectral frequencies by −1500 to +1500 Hz; this is a true frequency offset, not semitone transposition. **Mix** blends latency-matched dry and processed audio. **Bloom** adds energy from nearby spectral bins, **Blur** smooths neighboring bins, and **Low cut / High cut** set the frequency band being shifted.
- **Grain** page — **Grain mix** blends a granular delay after the spectral processor. **Grain size** sets each window from 25 to 240 ms, **Grain pitch** transposes grains, **Density** sets their trigger rate, and **Feedback** controls the bounded recirculation.
- **Mod** page — select an LFO waveform and destination, then set its **Rate** and **Depth**. The live scope shows phase and waveform.
- **Settings** page — choose a 2048, 4096, or 8192-point FFT, bypass each processing layer, and set **Output** level.
- The granular field on the left is an XY control: drag left/right to set spectral Shift and up/down to set Grain mix. Its subtle flower outline frames the moving grain particles.

The processor uses a custom radix-2 FFT with 4× overlap-add to form an analytic signal, then applies a phase-continuous frequency offset in Hz. FFT sizes are 2048, 4096, and 8192 points; reported host latency follows the selected window. A Nyquist guard removes content that would alias when shifting upward. The grain engine uses eight Hann-windowed playback voices per channel, interpolated reads, and a two-second circular buffer. Feedback saturation is confined to the feedback signal. Audio buffers are allocated before playback, so processing does not allocate memory on the audio thread. Controls are automatable and saved in the DAW project. The vector granular visualizer responds to input and LFO activity.

The editor keeps its existing layout and can be resized proportionally from 780 × 465 to 1560 × 930. Its graphite, warm coral, and pale-control palette uses system sans-serif typography, vector shapes, and text, so labels and edges stay clear at different sizes in hosts such as Ableton Live.

## Presets

The menu includes five factory presets: **Botanical Init**, **Leaf Veil**, **Pollen Drift**, **Glass Orchid**, and **Rain Memory**. Choose one to load its settings. To save a control setup, click **Save preset**, enter a name, then press **Save** or Enter. User presets are portable XML files (`.aupreset`) stored under `355ERA/Aura/Presets` in the user's application data folder. **Save bank** exports all saved user presets as a shareable `.aubank` file; **Load bank** imports that bank into the preset menu. Load bank also accepts a single `.aupreset` file. Aura presets remain available across sessions and are also saved inside the DAW project when the host saves the plugin state.

## Build on Windows

Install Visual Studio 2022 (Desktop development with C++), CMake 3.22 or newer, and Git. From this folder run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target Aura_VST3
```

The VST3 bundle is created under `build/Aura_artefacts/Release/VST3/Aura.vst3`. Copy it to your DAW's VST3 folder or configure CMake to install it after building. The macOS GitHub Actions artifact is built for Intel (x86_64); Apple Silicon users can run an Intel build of their DAW under Rosetta to load it.

To launch the optional standalone build, build `Aura_Standalone` instead. It provides a convenient way to audition the effect outside a DAW.

## Build on macOS or Linux

Use a C++17 compiler, CMake 3.22 or newer, and Git. On Ubuntu, install the build and GUI/audio development packages used by CI:

```sh
sudo apt-get update
sudo apt-get install -y build-essential libasound2-dev libfreetype6-dev \
  libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libgl1-mesa-dev
```

Then build with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Aura_VST3
```

CMake downloads the pinned JUCE 9.0.3 source on the first configure, so the first build needs network access. A GitHub Actions workflow is included to build the VST3 target on Windows, macOS Intel, and Linux, then attach each platform's VST3 bundle to the workflow run as a downloadable artifact. Aura does not bundle sample libraries; a VST3 bundle around a few megabytes is normal for a vector interface and native DSP, and file size does not measure audio quality.

## Add to GitHub

This project folder is already a Git repository on branch `main` with an initial commit. Create an empty repository on GitHub, then connect and push it:

```sh
git remote add origin https://github.com/YOUR-ACCOUNT/Aura.git
git push -u origin main
```

Replace `YOUR-ACCOUNT` with your GitHub username or organization. The workflow builds the plugin after pushes and pull requests and uploads each platform's VST3 bundle as a run artifact; it does not publish a release or install binaries.

If you start from the source ZIP instead, initialize Git and create the first commit before adding the remote:

```sh
git init --initial-branch=main
git add .
git commit -m "Add Aura VST3 spectral shifter"
git remote add origin https://github.com/YOUR-ACCOUNT/Aura.git
git push -u origin main
```

## Notes

- Aura processes incoming audio; it is an effect plugin, not a sample browser or sampler. Load it on an audio track or sample channel in your DAW.
- JUCE is fetched from its official repository at the pinned `9.0.3` tag. Review JUCE's licensing terms before distributing builds.
- The four-character VST3 manufacturer and plugin IDs in `CMakeLists.txt` should stay fixed after users have saved projects with Aura.
