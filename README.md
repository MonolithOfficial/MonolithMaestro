# MyFirstPlugin - JUCE VST3 Plugin

A minimal VST3 audio plugin built with JUCE framework. This plugin is a simple pass-through effect that demonstrates the basic structure of a JUCE plugin project.

## Features

- Pass-through audio processing (no effects applied)
- Simple GUI with title label
- VST3 format support
- Modern C++17
- CMake build system

## Requirements

- CMake 3.22 or higher
- C++17 compatible compiler (MSVC 2019+, GCC 9+, Clang 10+)
- JUCE framework (see installation below)

## Installation

### 1. Clone JUCE Framework

First, you need to get the JUCE framework. In the project root directory:

```bash
git clone https://github.com/juce-framework/JUCE.git
```

This will create a `JUCE` folder in your project root.

### 2. Build the Plugin

#### Windows (Visual Studio)

```bash
# Create build directory
mkdir build
cd build

# Generate Visual Studio project
cmake .. -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release
```

#### macOS/Linux

```bash
# Create build directory
mkdir build
cd build

# Generate makefiles
cmake ..

# Build
cmake --build . --config Release
```

## Project Structure

```
Monolith_Maestro/
├── CMakeLists.txt              # CMake build configuration
├── JUCE/                       # JUCE framework (clone separately)
├── Source/                     # Plugin source files
│   ├── PluginProcessor.h       # Audio processor header
│   ├── PluginProcessor.cpp     # Audio processor implementation
│   ├── PluginEditor.h          # GUI editor header
│   └── PluginEditor.cpp        # GUI editor implementation
└── README.md                   # This file
```

## Plugin Details

- **Plugin Name**: MyFirstPlugin
- **Company**: MonolithMaestro
- **Format**: VST3
- **Category**: Effect (Fx)
- **Manufacturer Code**: Mnlt
- **Plugin Code**: Mfp1

## Customization

To customize the plugin:

1. **Change plugin name**: Edit `PRODUCT_NAME` in `CMakeLists.txt`
2. **Add parameters**: Modify `PluginProcessor.h/cpp` to add audio parameters
3. **Modify GUI**: Update `PluginEditor.h/cpp` to change the interface
4. **Add processing**: Implement your audio algorithm in `processBlock()` method

## Building for Different Formats

To add support for AU, AAX, or Standalone formats, modify the `FORMATS` line in `CMakeLists.txt`:

```cmake
FORMATS VST3 AU Standalone  # Example: add AU and Standalone
```

## License

This is a template project. Add your own license as needed.

## Next Steps

- Add audio parameters (gain, filter cutoff, etc.)
- Implement actual audio processing
- Create a more sophisticated GUI
- Add preset management
- Implement state save/load functionality

## Resources

- [JUCE Framework Documentation](https://docs.juce.com/)
- [JUCE Tutorials](https://juce.com/learn/tutorials)
- [JUCE Forum](https://forum.juce.com/)
