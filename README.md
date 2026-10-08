# Aura

**Aura** is a stereo/mono VST3 FFT spectral shifter by **355ERA**. It is designed as an insert effect for samples, field recordings, and instruments, with a soft botanical look suited to organic and botanica sound design.

![Aura interface preview](Aura-interface-preview.svg)

## Controls

- **Shift** — pitch-shifts the spectrum in one-semitone steps from −24 to +24. Double-click the knob to return to zero.
- **Mix** — blends the latency-matched dry signal with the shifted signal.
- **Bloom** — softly diffuses neighboring spectral bins for a gentler, more misty tone.

The processor uses a custom 2048-point radix-2 FFT, 4× overlap-add, per-bin phase tracking, and spectral-bin remapping. Bloom softly diffuses neighboring bins. The plugin reports its 2048-sample processing latency to the host. Its controls are automatable and their values are saved in the DAW project. The lily visualizer blooms with the input signal level.

## Build on Windows

Install Visual Studio 2022 (Desktop development with C++), CMake 3.22 or newer, and Git. From this folder run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target Aura_VST3
```

The VST3 bundle is created under `build/Aura_artefacts/Release/VST3/Aura.vst3`. Copy it to your DAW's VST3 folder or configure CMake to install it after building.

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

CMake downloads the pinned JUCE 9.0.3 source on the first configure, so the first build needs network access. A GitHub Actions workflow is included to build the VST3 target on Windows, macOS, and Linux, then attach each platform's VST3 bundle to the workflow run as a downloadable artifact.

## Add to GitHub

Create an empty repository on GitHub, then connect this local Git repository and push the `main` branch:

```sh
git remote add origin https://github.com/YOUR-ACCOUNT/Aura.git
git push -u origin main
```

Replace `YOUR-ACCOUNT` with your GitHub username or organization. The workflow builds the plugin after pushes and pull requests and uploads each platform's VST3 bundle as a run artifact; it does not publish a release or install binaries.

## Notes

- Aura processes incoming audio; it is an effect plugin, not a sample browser or sampler. Load it on an audio track or sample channel in your DAW.
- JUCE is fetched from its official repository at the pinned `9.0.3` tag. Review JUCE's licensing terms before distributing builds.
- The four-character VST3 manufacturer and plugin IDs in `CMakeLists.txt` should stay fixed after users have saved projects with Aura.
