# C++ Prototype Status (December 22, 2025)

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

### 5. FM Demodulator (`src/demod/fm_demodulator.cpp`) - UPDATED
- Phase extraction via `std::arg()`
- Phase difference computation (matching Python's `np.ediff1d`)
- Phase unwrapping to [-π, π]
- Clamping to [0, 2π] for bad data handling
- Frequency scaling: `freq = phase_diff * (sampleRate / 2π)`
- **Note**: Removed envelope gating (now matches Python exactly)
- **Known issue**: Edge effects at block boundaries cause 0 Hz values

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

### 7. TBC Scaler (`src/tbc/tbc_scaler.cpp`)
- Resamples from input rate (40 MHz) to output rate (4×fsc = 17.734475 MHz)
- Input line length: 2560 samples (64µs × 40 MHz)
- Output line length: 1135 samples (64µs × 17.734475 MHz)
- Bicubic interpolation matching Python's `scale()` function

### 8. TBC Writer (`src/output/tbc_writer.cpp`)
- Writes .tbc video files (16-bit samples)
- Writes _chroma.tbc chroma files
- Writes .tbc.json metadata with proper PAL parameters

### 9. Horizontal Sync Detection (`src/sync/sync_detector.cpp`) - UPDATED
- Finds horizontal sync pulses in demodulated video
- Computes line start positions from pulse locations
- Pulse filtering: 80-1200 samples length, 94-376 for HSYNC candidates
- Line length validation: ±10% of expected 2560 samples
- **Fallback to fixed positions when <80% of expected lines detected**
- **Result**: 218/312 lines (70%) have sync in first 100 samples

### 10. Vertical Sync Detection (`src/sync/vsync_detector.cpp`)
- Detects field boundaries by analyzing the vertical blanking interval
- State machine matching Python's `run_vblank_state_machine()`:
  - HSYNC_SEARCH → EQPL1 → VSYNC → EQPL2 → DONE
- PAL pulse timing thresholds (at 40 MHz):
  - HSYNC: 112-300 samples (4.7µs nominal)
  - EQ: 56-150 samples (2.35µs nominal)
  - VSYNC: 655-1747 samples (27.3µs nominal)
- **Results on test file (out1.u8):**
  - Detected 1718 pulses: HSYNC=1251, EQ=42, VSYNC=21
  - Found 4 field boundaries
  - First field at sample 191190 (line ~74)
  - Field length: 798735 samples (~312 lines) ✓

## Current Issues (December 22, 2025)

### 1. FM Demodulator Edge Effects
- First few samples of each block produce 0 Hz (clips to 0 digital value)
- Caused by bandpass filter transient response at block boundaries
- **Fix needed**: Overlap-save processing to eliminate edge effects

### 2. Incomplete Sync Detection
- Only 223/312 lines detected (71.5%)
- 47 line intervals outside ±10% tolerance
- EQ pulses at 94 samples are at the edge of the filter threshold
- **Current mitigation**: Fall back to fixed line positions

### 3. Python Decoder Comparison Blocked
- Python decoder fails on out1.u8 with "Level detection failed - sync or blank is None"
- Unable to generate reference output for direct comparison
- Need valid VHS capture that both decoders can process

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
- ✓ Vertical sync detection (field boundaries)

### What's Missing for Full Match
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

Vertical sync detection:
  Detected 1718 pulses: HSYNC=1251, EQ=42, VSYNC=21
  Found 4 field boundaries in ~3 fields of data
  First field at sample 191190 (line ~74)
  Field length: 798735 samples (~312 lines) ✓

Horizontal sync detection (first field):
  Pulses found: 313
  Lines detected: 223
  Detected line length: 2468 samples (expected ~2560)
  First 3 line starts: 148, 2616, 5175
```

### Line Alignment Verification
```
Line 0: sync min at pos 1-2
Line 1: sync min at pos 1-2
Line 2: sync min at pos 2-3
```
Lines are now aligned to horizontal sync!

## Missing Components (Not Yet Implemented)

### 1. Sub-Sample Line Positioning (HIGH PRIORITY)
- Python uses `linelocs` with interpolation for precise positioning
- C++ uses fixed-length lines (2560 samples)
- Need to implement per-line position tracking and interpolation
- **Impact**: This affects per-line horizontal alignment

### 2. Time-Base Correction Refinement
- Basic resampling implemented (2560 → 1135)
- Missing: wow/flutter compensation
- Missing: per-line variable timing adjustment

### 3. Signal Enhancement
- High-frequency boost in weak signal areas (Python's `high_boost`)
- Spike detection and replacement (Python's `diff_demod`)
- Better envelope-based processing

### 4. Dropout Detection/Compensation
- Use envelope signal to detect dropouts
- Replace dropout samples with interpolated data

### 5. Color Processing
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
