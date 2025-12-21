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

### 6. Video Scaling (Updated December 21, 2025)
- VHS PAL frequency-to-IRE mapping (from vhsdecode/format_defs/vhs.py):
  - `ire0` = 4.1 MHz (0 IRE / black level)
  - `hz_ire` = 7000 Hz per IRE
  - `vsync_ire` = -42.857 IRE (PAL standard)
  - Sync tip at 3.8 MHz (-42.857 IRE)
  - Peak white at 4.8 MHz (100 IRE)
- Digital scaling (from lddecode/core.py FieldPAL):
  - `outputZero` = 256 (sync tip level)
  - `out_scale` = 53760 / 142.857 = 376.32
  - Formula: `output = (ire - vsync_ire) * out_scale + outputZero`
  - -42.857 IRE → 256
  - 0 IRE → 16384
  - 100 IRE → 54016

### 7. TBC Scaler (`src/tbc/tbc_scaler.cpp`) ← NEW
- Resamples from input rate (40 MHz) to output rate (4×fsc = 17.734475 MHz)
- Input line length: 2560 samples (64µs × 40 MHz)
- Output line length: 1135 samples (64µs × 17.734475 MHz)
- Bicubic interpolation matching Python's `scale()` function

### 8. TBC Writer (`src/output/tbc_writer.cpp`)
- Writes .tbc video files (16-bit samples)
- Writes _chroma.tbc chroma files
- Writes .tbc.json metadata with proper PAL parameters

### 9. Sync Detection (`src/sync/sync_detector.cpp`)
- Finds horizontal sync pulses in demodulated video
- Computes line start positions from pulse locations
- Aligns video lines to sync pulses for proper TBC output
- **Results on test file:**
  - 312 pulses found per field
  - 211 lines detected (expected: 312)
  - Detected line length: 2558 samples (expected: 2560)
  - Sync positions now consistent at pos 1-3

## Comparison with Python Decoder

### Current Status (December 21, 2025)
When comparing C++ output with Python vhs-decode output on the same input file:

| Metric | Python | C++ | Match |
|--------|--------|-----|-------|
| Output line length | 1135 samples | 1135 samples | ✓ |
| Output sample rate | 17.734475 MHz | 17.734475 MHz | ✓ |
| Mean digital value | ~19857 | ~19196 | Close |
| Value range | 0-52200 | 0-65535 | C++ has more clipping |
| **Field correlation** | - | 0.37-0.46 | Low |

### Why Correlation is Low

1. **Field alignment**: Python starts decoding at file position 983040 (finding first valid field via vertical sync). C++ starts at block-aligned position. Use `--seek` option to align.

2. **Line-level offset**: Python and C++ fields are offset by ~16-30 lines even when starting at same position. This is due to different vertical sync detection.

3. **Horizontal alignment**: Both detect sync pulses, but Python uses sub-sample interpolation for precise line positioning.

4. **Additional Python processing**:
   - High-frequency boost in weak signal areas
   - Spike replacement with diff demod
   - More sophisticated envelope-based processing

### What's Working in C++
- ✓ RF bandpass filtering (1.3-5.78 MHz)
- ✓ FM demodulation (frequency mean matches Python: ~4.15 MHz)
- ✓ IRE-to-digital scaling (correct formula)
- ✓ Output at correct TBC format (1135 × 312 samples)
- ✓ Bicubic interpolation for resampling
- ✓ Horizontal sync detection

### What's Missing for Full Match
- ✗ Vertical sync detection (field boundaries)
- ✗ Sub-sample line positioning (linelocs interpolation)
- ✗ High-frequency boost
- ✗ Spike replacement
- ✗ Proper dropout handling

## Test Results

### With Valid RF Capture (out1.u8)
```
Raw input stats (uint8): min=31 max=223 avg=126.1 range=192
Envelope stats: Min=0.27, Max=0.73, Avg=0.50
Video frequency: Min=0, Max=5.6MHz, Avg=4.1MHz (7.0 IRE)

Sync detection (first field):
  Pulses found: 312
  Lines detected: 211
  Detected line length: 2558 samples
  First 3 line starts: 1368, 3926, 6483 (spacing ~2558 ✓)
```

### Line Alignment Verification
```
Line 0: sync min at pos 1-2
Line 1: sync min at pos 1-2
Line 2: sync min at pos 2-3
```
Lines are now aligned to horizontal sync!

## Missing Components (Not Yet Implemented)

### 1. Vertical Sync Detection (HIGH PRIORITY)
- Identify field boundaries (currently starts at arbitrary position)
- Detect first/second field for proper interlacing
- Handle non-standard sync patterns
- **Impact**: This is the main cause of low correlation with Python

### 2. Sub-Sample Line Positioning
- Python uses `linelocs` with interpolation for precise positioning
- C++ uses fixed-length lines (2560 samples)
- Need to implement per-line position tracking and interpolation

### 3. Time-Base Correction Refinement
- Basic resampling implemented (2560 → 1135)
- Missing: wow/flutter compensation
- Missing: per-line variable timing adjustment

### 4. Signal Enhancement
- High-frequency boost in weak signal areas (Python's `high_boost`)
- Spike detection and replacement (Python's `diff_demod`)
- Better envelope-based processing

### 5. Dropout Detection/Compensation
- Use envelope signal to detect dropouts
- Replace dropout samples with interpolated data

### 6. Color Processing
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
- `--seek N` - Skip to byte offset in input file (for alignment with Python)
- `--gpu` - Enable GPU acceleration (OpenCL)
- `--threads N` - Number of threads

### Aligning with Python Output

To start decoding at the same position as Python, find the first field's `fileLoc` from Python's JSON output:

```bash
# Decode with Python first
python decode.py vhs --system PAL --length 3 input.u8 python_output

# Check starting position
python -c "import json; print(json.load(open('python_output.tbc.json'))['fields'][0]['fileLoc'])"
# Output: 983040

# Decode with C++ starting at same position
./src/vhs-decode --system PAL --length 3 --seek 983040 input.u8 cpp_output
```

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
│   │   └── sync_detector.cpp  # Horizontal sync detection
│   ├── tbc/
│   │   └── tbc_scaler.cpp     # Time-base correction resampling
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
