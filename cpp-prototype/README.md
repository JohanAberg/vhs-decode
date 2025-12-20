# VHS-Decode C++ Prototype

This is a working prototype for the C++ rewrite of VHS-Decode, focusing initially on VHS format with a modular architecture for adding more formats.

## Current Status

**Phase 1: Foundation** (Complete)
- ✅ Project structure created
- ✅ CMake build system
- ✅ Core type definitions
- ✅ Modular format interface
- ✅ VHS format implementation
- ✅ RF reader with memory-mapped I/O
- ✅ FFT engine (CPU prototype)
- ✅ FM demodulator (phase unwrap, envelope)
- ✅ TBC writer (video, chroma, metadata)

**Phase 2: RF Processing** (Complete)
- ✅ Filter bank (bandpass, lowpass, highpass)
- ✅ Hilbert transform (analytic signal generation)
- ✅ RF processor pipeline orchestrator
- ✅ Frequency-domain filtering
- ✅ Integrated RF → FFT → Filter → Hilbert → iFFT pipeline

## Architecture

The prototype uses a modular, extensible architecture:

```
FormatBase (Abstract Interface)
    ├── VHSFormat (Implemented)
    ├── SVHSFormat (TODO)
    ├── BetamaxFormat (TODO)
    └── Video8Format (TODO)
```

Each format implementation provides:
- System parameters (NTSC/PAL/etc.)
- Carrier frequencies
- Processing configuration
- Format-specific settings

## Building

### Linux/macOS

```bash
cd cpp-prototype
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./vhs-decode --help
```

### Windows

```bash
cd cpp-prototype
mkdir build && cd build
cmake ..
cmake --build . --config Release
Release\vhs-decode.exe --help
```

## Usage

```bash
# Basic VHS NTSC decode
./vhs-decode --format VHS --system NTSC input.r40 output.tbc

# VHS PAL decode with GPU
./vhs-decode --format VHS --system PAL --gpu input.r40 output.tbc

# Multi-threaded decode
./vhs-decode --format VHS --system NTSC --threads 8 input.r40 output.tbc
```

## Current Features

### Implemented (Phase 1 + 2)
- ✅ Modular format system
- ✅ VHS format support (NTSC, PAL, PAL-M)
- ✅ System parameter calculation
- ✅ Carrier frequency configuration
- ✅ Command-line interface
- ✅ Format/system auto-detection from parameters
- ✅ RF file reader with memory-mapped I/O
- ✅ Cross-platform file handling (Linux/Windows/macOS)
- ✅ FFT engine (CPU-based Cooley-Tukey implementation)
- ✅ FM demodulator (phase unwrapping, envelope detection)
- ✅ TBC file writer (video, chroma, JSON metadata)
- ✅ **Filter bank** (bandpass, lowpass, highpass filters)
- ✅ **Hilbert transform** (analytic signal generation)
- ✅ **RF processor** (integrated pipeline orchestrator)

### TODO (Phase 3+: Next Steps)
- ⏳ Replace prototype FFT with FFTW3 library
- ⏳ Add clFFT for GPU acceleration (OpenCL)
- ⏳ Optimize phase unwrapping with custom kernels
- ⏳ Video/chroma separation filters
- ⏳ Dropout detection and correction
- ⏳ Time-base correction
- ⏳ Full integrated decode pipeline
- ⏳ Multi-threaded block processing

## Adding New Formats

To add support for a new tape format:

1. Create header file: `include/formats/new_format.hpp`
2. Inherit from `FormatBase`
3. Implement required methods:
   - `getFormatParams()`
   - `getSystemParams(TVSystem)`
   - `getVideoCarrierMHz(TVSystem)`
   - `getChromaCarrierMHz(TVSystem)`
   - `createConfig(TVSystem, inputFreqMHz)`

4. Add to factory in `formats/format_base.cpp`
5. Update `formatFromString()` parser

Example:
```cpp
class BetamaxFormat : public FormatBase {
public:
    TapeFormat getFormat() const override { return TapeFormat::BETAMAX; }
    // ... implement other methods
};
```

## Directory Structure

```
cpp-prototype/
├── CMakeLists.txt              # Root build configuration
├── include/
│   ├── vhsdecode/
│   │   ├── types.hpp           # Common types and structures
│   │   ├── rf_reader.hpp       # RF file reader interface
│   │   ├── tbc_writer.hpp      # TBC file writer interface
│   │   ├── fft_engine.hpp      # FFT engine interface
│   │   ├── fm_demodulator.hpp  # FM demodulator interface
│   │   ├── filter_bank.hpp     # Filter bank interface (Phase 2)
│   │   ├── hilbert.hpp         # Hilbert transform (Phase 2)
│   │   └── rf_processor.hpp    # RF processor orchestrator (Phase 2)
│   └── formats/
│       ├── format_base.hpp     # Abstract format interface
│       └── vhs_format.hpp      # VHS format implementation
├── src/
│   ├── CMakeLists.txt          # Source build configuration
│   ├── core/
│   │   └── main.cpp            # Application entry point
│   ├── formats/
│   │   ├── format_base.cpp     # Format utilities
│   │   └── vhs_format.cpp      # VHS implementation
│   ├── io/
│   │   ├── rf_reader.cpp       # RF reader implementation
│   │   └── tbc_writer.cpp      # TBC writer implementation
│   ├── rf/
│   │   ├── fft_engine.cpp      # FFT engine implementation
│   │   ├── filter_bank.cpp     # Filter bank implementation (Phase 2)
│   │   ├── hilbert.cpp         # Hilbert transform (Phase 2)
│   │   └── rf_processor.cpp    # RF processor (Phase 2)
│   └── demod/
│       └── fm_demodulator.cpp  # FM demodulator implementation
├── tests/                      # Unit tests (TODO)
└── README.md                   # This file
```

## Design Principles

1. **Modularity**: Each format is self-contained and independent
2. **Extensibility**: Easy to add new formats without modifying existing code
3. **Type Safety**: Strong typing with enums and structs
4. **Modern C++**: Using C++17 features (smart pointers, structured bindings, etc.)
5. **Performance**: Designed for OpenCL GPU acceleration
6. **Portability**: Cross-platform (Linux, Windows, macOS)

## Testing

```bash
cd build
ctest
```

Or test the prototype manually:

```bash
./vhs-decode --format VHS --system NTSC dummy.r40 dummy.tbc
```

This will show the configuration without actually decoding (decoding not yet implemented).

## Next Development Steps

1. **RF Reader** (`src/io/rf_reader.cpp`)
   - Memory-mapped file I/O
   - Block-based reading
   - Support for different sample formats (u8, i16, f32)

2. **FFT Engine** (`src/rf/fft_engine.cpp`)
   - CPU path with FFTW3
   - GPU path with clFFT
   - Automatic fallback

3. **FM Demodulator** (`src/rf/fm_demod.cpp`)
   - Phase unwrapping
   - Envelope detection
   - Spike replacement

4. **TBC Writer** (`src/io/tbc_writer.cpp`)
   - 16-bit packed format
   - JSON metadata
   - Chroma output (optional)

5. **Pipeline Integration**
   - Connect all components
   - Multi-threaded block processing
   - Progress reporting

## Contributing

This is a prototype demonstrating the modular architecture. Contributions welcome!

Focus areas:
- Implementing missing components
- Adding new format support (SVHS, Betamax, Video8)
- Optimizing performance
- Adding tests

## License

Same as main VHS-Decode project (GPL).

## See Also

- [Full Rewrite Plan](../CPP_OPENCL_REWRITE_PLAN.md)
- [Executive Summary](../CPP_REWRITE_SUMMARY.md)
- [Documentation Index](../CPP_REWRITE_INDEX.md)
- [Main VHS-Decode Repository](https://github.com/oyvindln/vhs-decode)
