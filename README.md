# Monolith Maestro - Real-Time Pitch Detection VST3

An AI-powered VST3 audio plugin that performs real-time polyphonic pitch detection. Analyze incoming audio from any instrument (guitar, piano, vocals) and display detected notes with high accuracy.

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![JUCE](https://img.shields.io/badge/JUCE-7.x-orange)
![C++](https://img.shields.io/badge/C%2B%2B-17-green)

## Features

- **Real-Time Pitch Detection**: FFT-based analysis detects notes as you play
- **Polyphonic Support**: Detects up to 4 simultaneous notes
- **Harmonic Filtering**: Intelligent algorithm isolates fundamental frequencies
- **Visual Feedback**: Clean, modern GUI displays detected notes with frequency and MIDI info
- **Low Latency**: Optimized for real-time performance (2048-sample FFT)
- **Instrument Agnostic**: Works with guitar, piano, vocals, or any melodic instrument

## Requirements

- **CMake**: 3.22 or higher
- **Compiler**: C++17 compatible (MSVC 2019+, GCC 9+, Clang 10+)
- **JUCE Framework**: 7.x (see installation below)
- **Platform**: Windows, macOS, or Linux

## Installation

### 1. Clone JUCE Framework

In the project root directory:

```bash
git clone https://github.com/juce-framework/JUCE.git
```

### 2. Build the Plugin

#### Windows (Visual Studio)

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

The compiled VST3 will be located at:
```
build/MonolithMaestro_artefacts/Release/VST3/MonolithMaestro.vst3
```

#### macOS/Linux

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 3. Install the Plugin

Copy the built VST3 to your system's plugin directory:

**Windows**: `C:\Program Files\Common Files\VST3\`
**macOS**: `~/Library/Audio/Plug-Ins/VST3/`
**Linux**: `~/.vst3/`

## Usage

1. Load **Monolith Maestro** in your DAW (tested with FL Studio 12)
2. Insert it on a mixer track with your instrument input
3. Play your instrument - notes will be detected and displayed in real-time
4. The GUI shows:
   - **Note name** (e.g., "C4", "A#5")
   - **Frequency** in Hz
   - **MIDI note number**
   - **Magnitude bar** (signal strength)

## Project Structure

```
Monolith_Maestro/
├── CMakeLists.txt              # Build configuration
├── JUCE/                       # JUCE framework (clone separately)
├── Source/
│   ├── PluginProcessor.h/cpp   # Main audio processor
│   ├── PluginEditor.h/cpp      # GUI interface
│   └── PitchDetector.h/cpp     # FFT-based pitch detection engine
├── CLAUDE.md                   # Detailed project documentation
├── JUCE_EXPLAINED.md           # JUCE/C++ learning guide
└── README.md                   # This file
```

## Technical Details

### Audio Processing Pipeline

1. **Input**: Mono audio (left channel) from DAW
2. **FIFO Buffer**: Accumulates samples until 2048 available
3. **Windowing**: Applies Hann window to reduce spectral leakage
4. **FFT**: Converts time-domain to frequency-domain (2048-point FFT)
5. **Peak Detection**: Finds local maxima in magnitude spectrum
6. **Harmonic Filtering**: Removes harmonics (2x, 3x, 4x fundamentals)
7. **Note Conversion**: Frequency → MIDI note → Note name
8. **Output**: Displays up to 4 strongest detected notes

### Configuration

- **FFT Size**: 2048 samples (~46ms at 44.1kHz, ~21Hz resolution)
- **Noise Gate**: 0.001 RMS threshold
- **Magnitude Threshold**: 0.02 (filters weak harmonics)
- **Max Polyphony**: 4 simultaneous notes
- **Update Rate**: 20Hz GUI refresh

### Plugin Metadata

- **Plugin Name**: Monolith Maestro
- **Company**: MonolithMaestro
- **Format**: VST3
- **Category**: Effect (Fx)
- **Manufacturer Code**: Mnlt
- **Plugin Code**: MnMa

## Customization

### Adjust Detection Sensitivity

Edit `PluginProcessor.cpp` in `prepareToPlay()`:

```cpp
pitchDetector_.setNoiseGateThreshold(0.001f);   // Lower = more sensitive to quiet sounds
pitchDetector_.setMagnitudeThreshold(0.02f);    // Lower = detects weaker harmonics
```

### Change FFT Size

Edit `PitchDetector.h`:

```cpp
static constexpr int fftOrder_ = 11;  // 11 = 2048 samples, 12 = 4096 samples
```

Higher FFT size = better frequency resolution, higher latency.

### Modify Max Notes

Edit `PitchDetector.h`:

```cpp
static constexpr int maxPolyphony_ = 4;  // Detect up to N simultaneous notes
```

## Future Roadmap

- [ ] Audio sequence recording
- [ ] Google Gemini API integration for AI melody suggestions
- [ ] MIDI output for detected notes
- [ ] Tuner mode with cents deviation display
- [ ] Preset system for different instruments
- [ ] Waveform/spectrum visualization
- [ ] VST2, AU, AAX format support

## Troubleshooting

### Plugin doesn't appear in DAW

- Ensure VST3 is in the correct plugin directory
- Rescan plugins in your DAW
- Check that build was in **Release** mode (not Debug)

### Notes not detected

- Increase input gain in your audio interface
- Play louder/clearer notes
- Lower thresholds in `prepareToPlay()`
- Check that your DAW is routing audio to the plugin

### Build errors

- Verify JUCE is cloned at project root: `Monolith_Maestro/JUCE/`
- Check CMake version: `cmake --version` (needs 3.22+)
- Ensure compiler supports C++17

## Resources

- **JUCE Documentation**: https://docs.juce.com/
- **JUCE Forum**: https://forum.juce.com/
- **Google Gemini API**: https://ai.google.dev/
- **DSP Guide**: http://www.dspguide.com/

## License

This project is provided as-is. Add your own license as needed.

## Credits

Built with ❤️ using JUCE Framework
Pitch detection algorithm uses FFT with harmonic filtering
