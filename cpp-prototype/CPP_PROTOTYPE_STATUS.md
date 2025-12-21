# C++ Prototype Status (December 21, 2025)

## Overview

The C++ prototype implements the core VHS RF decoding pipeline with CPU-based processing.
It serves as a foundation for GPU acceleration via OpenCL.

## Working Components

### 1. RF File Reader (`src/rf/rf_reader.cpp`)
- Memory-mapped file I/O for efficient large file handling
- Supports uint8 (.u8) input format
- Block-based reading with configurable block size (default 32768 samples)

### 2. FFT Engine (`src/fft/fft_engine.cpp`)
- FFTW3 (single-precision) for high-performance FFT/iFFT
- Forward FFT: real-to-complex
- Inverse FFT: complex-to-real
- Reconstruction error: ~1.5e-7

### 3. Filter Bank (`src/rf/filter_bank.cpp`)
- Frequency-domain filter creation and application
- Bandpass, lowpass, highpass filter generation
- VHS PAL RF bandpass: 1.3 - 5.78 MHz (from Python vhsdecode/format_defs/vhs.py)

### 4. Hilbert Transform (`src/rf/hilbert.cpp`)
- Generates analytic signal from real RF data
- Standard frequency-domain implementation:
  - DC component: multiply by 1
  - Positive frequencies: multiply by 2
  - Negative frequencies: multiply by 0

### 5. FM Demodulator (`src/demod/fm_demodulator.cpp`)
- Phase extraction via `std::arg()`
- Phase difference computation (matching Python's `np.ediff1d`)
- Phase unwrapping to [-π, π]
- Clamping to [0, 2π] for bad data handling
- Frequency scaling: `freq = phase_diff * (sampleRate / 2π)`
- Envelope-based gating for low-signal areas

### 6. Video Scaling
- VHS PAL frequency-to-IRE mapping:
  - `ire0` = 4.085714 MHz (0 IRE / black level)
  - `hz_ire` = 7142.86 Hz per IRE
  - Sync tip (-40 IRE) at 3.8 MHz
  - Peak white (100 IRE) at 4.8 MHz
- Digital scaling: 0 IRE → 16384, 100 IRE → 60160

### 7. TBC Writer (`src/output/tbc_writer.cpp`)
- Writes .tbc video files (16-bit samples)
- Writes _chroma.tbc chroma files
- Writes .tbc.json metadata with proper PAL parameters

### 8. Sync Detection (`src/sync/sync_detector.cpp`) ← NEW
- Finds horizontal sync pulses in demodulated video
- Computes line start positions from pulse locations
- Aligns video lines to sync pulses for proper TBC output
- **Results on test file:**
  - 325 pulses found per field
  - 301 lines detected (expected: 312)
  - Detected line length: 2558 samples (expected: 2560)
  - Sync positions now consistent at pos 2-3 (was random before)

## Test Results

### With Valid RF Capture (out1.u8)
```
Raw input stats (uint8): min=31 max=223 avg=126.1 range=192
Envelope stats: Min=0.27, Max=0.73, Avg=0.50
Video frequency: Min=0, Max=5.6MHz, Avg=4.1MHz (8.9 IRE)

Sync detection (first field):
  Pulses found: 325
  Lines detected: 301
  Detected line length: 2558 samples
  First 3 line starts: 1367, 3925, 6482 (spacing ~2558 ✓)
```

### Line Alignment Verification
```
Line 0: sync min at pos 2
Line 1: sync min at pos 2
Line 2: sync min at pos 3
Line 3: sync min at pos 2
Line 4: sync min at pos 2
```
Lines are now aligned to horizontal sync!

## Missing Components (Not Yet Implemented)

### 1. Time-Base Correction (NEXT PRIORITY)
- Compensate for tape speed variations
- Interpolate samples to exact line length (resample 2560 → 1135)
- Handle wow and flutter

### 2. Vertical Sync Detection
- Identify field boundaries
- Detect first/second field
- Handle non-standard sync patterns

### 3. Dropout Detection/Compensation
- Use envelope signal to detect dropouts
- Replace dropout samples with interpolated data

### 4. Color Processing
- Color burst detection and phase measurement
- Chroma separation (currently just copies luma)
- PAL/NTSC color decoding

## Build Instructions

```bash
cd cpp-prototype
mkdir -p build && cd build
cmake ..
cmake --build . --parallel 8
```

## Usage

```bash
./src/vhs-decode --system PAL --length 10 input.u8 output
```

Options:
- `--system PAL|NTSC` - TV system
- `--format VHS|SVHS|Betamax` - Tape format  
- `--length N` - Number of frames to decode (0 = all)
- `--gpu` - Enable GPU acceleration (OpenCL)
- `--threads N` - Number of threads

## File Structure

```
cpp-prototype/
├── src/
│   ├── core/main.cpp          # Entry point, argument parsing
│   ├── rf/
│   │   ├── rf_reader.cpp      # Memory-mapped file reader
│   │   ├── rf_processor.cpp   # RF processing pipeline
│   │   ├── filter_bank.cpp    # Frequency-domain filters
│   │   └── hilbert.cpp        # Hilbert transform
│   ├── demod/
│   │   └── fm_demodulator.cpp # FM demodulation
│   ├── sync/
│   │   └── sync_detector.cpp  # Horizontal sync detection (NEW)
│   ├── fft/
│   │   └── fft_engine.cpp     # FFTW3 wrapper
│   ├── output/
│   │   └── tbc_writer.cpp     # TBC file output
│   └── gpu/
│       └── filter_kernel.cpp  # OpenCL kernels (placeholder)
├── include/vhsdecode/         # Header files
├── tests/                     # Unit tests
└── tools/                     # Benchmark tools
```

## Dependencies

- FFTW3 (fftw3f for single-precision)
- OpenCL (optional, for GPU acceleration)
- C++17 compatible compiler
- CMake 3.16+

## Next Steps

1. ~~**Implement sync detection**~~ ✓ - Find horizontal sync pulses and align lines
2. **Add time-base correction** - Resample 2560 samples → 1135 (4×fsc rate)
3. **Implement vertical sync** - Proper field detection
4. **GPU acceleration** - Port critical paths to OpenCL
