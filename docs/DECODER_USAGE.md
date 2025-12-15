# VHS-Decode Quick Start Guide

## Basic Usage

### CPU Decoding (Standard)

```bash
# Basic decode with automatic system detection
python decode.py vhs input.r40 output

# Specify TV system (PAL/NTSC/NTSC-J/PAL-M)
python decode.py vhs --system PAL input.r40 output

# Specify sample frequency (default: 40 MSPS)
python decode.py vhs --system PAL -f 40 input.r40 output

# Control thread count (default: CPU count)
python decode.py vhs --system PAL --threads 8 input.r40 output

# Decode specific number of frames
python decode.py vhs --system PAL --length 100 input.r40 output

# Overwrite existing output
python decode.py vhs --system PAL --overwrite input.r40 output
```

### GPU Decoding (Accelerated)

**Requirements:**
- NVIDIA GPU with CUDA 11.0+ or 12.0+
- CuPy installed: `pip install cupy-cuda12x` (or `cupy-cuda11x`)

```bash
# Enable GPU acceleration
python decode.py vhs --system PAL --gpu input.r40 output

# GPU with specific thread count (recommended: 1-4 threads)
python decode.py vhs --system PAL --gpu --threads 4 input.r40 output

# GPU decode with limited frames (for testing)
python decode.py vhs --system PAL --gpu --length 50 input.r40 output
```

**GPU Performance Tips:**
- Use fewer threads (1-4) for GPU mode - more threads doesn't help
- Ensure input file is on fast storage (SSD recommended)
- Monitor GPU usage with: `nvidia-smi -l 1`

### Common Options

```bash
# Tape format (VHS, SVHS, Betamax, Video8, Hi8)
--tape-format SVHS

# Start at specific position (in RF samples)
--start 1000000

# Audio processing
--analog_audio    # Enable analog audio decoding

# Advanced options
--MTF 0.5        # Adjust MTF compensation (0.0-2.0)
--high_boost     # Enable high frequency boost
--recheck_phase  # Enable phase recheck (for difficult tapes)
```

## Setup

### Installation

```bash
# Clone repository
git clone https://github.com/oyvindln/vhs-decode.git
cd vhs-decode

# Create virtual environment (recommended)
python -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# For GPU support (CUDA 12.x)
pip install cupy-cuda12x

# Or for CUDA 11.x
pip install cupy-cuda11x
```

### Verify Installation

```bash
# Check if decoder works
python decode.py --help

# Check GPU availability (if installed)
python -c "from vhsdecode.gpu_utils import GPU_AVAILABLE; print('GPU Available:', GPU_AVAILABLE)"

# Check CUDA version
nvidia-smi
```

## Examples

### Example 1: Quick Test Decode

```bash
# Decode first 10 frames to test setup
python decode.py vhs --system PAL --length 10 sample.r40 test_output
```

### Example 2: Full PAL Tape with GPU

```bash
# Full decode with GPU acceleration
python decode.py vhs --system PAL --gpu --threads 2 pal_tape.r40 pal_output

# Expected output files:
# - pal_output.tbc         (time-base corrected video)
# - pal_output.tbc.json    (metadata)
# - pal_output_chroma.tbc  (chroma data)
```

### Example 3: NTSC SVHS Tape

```bash
# SVHS with NTSC system
python decode.py vhs --system NTSC --tape-format SVHS svhs_tape.r40 svhs_output
```

### Example 4: Benchmark CPU vs GPU

```bash
# CPU baseline
time python decode.py vhs --system PAL --threads 8 --length 100 input.r40 cpu_out

# GPU comparison
time python decode.py vhs --system PAL --gpu --threads 2 --length 100 input.r40 gpu_out

# Compare outputs (should be identical within FP precision)
```

## Performance Expectations

### CPU Performance (40 MSPS PAL)
- **Single thread:** 0.5-1.5 FPS
- **8 threads:** 2.0-3.5 FPS
- **16 threads:** 2.5-4.0 FPS (diminishing returns)

### GPU Performance (RTX 3070+)
- **GPU + 2 threads:** 3.5-6.0 FPS
- **GPU + 4 threads:** 4.0-6.5 FPS
- **Speedup:** 1.5-2.5x vs CPU (varies by GPU)

**Note:** GPU performance improved with December 2025 bug fixes. Earlier versions had issues causing slowdowns.

## Troubleshooting

### GPU Not Working

```bash
# Check if CuPy is installed
python -c "import cupy as cp; print(cp.__version__)"

# If error "ModuleNotFoundError: No module named 'cupy'"
pip install cupy-cuda12x  # Adjust for your CUDA version

# Check CUDA version
nvidia-smi | findstr "CUDA Version"  # Windows
nvidia-smi | grep "CUDA Version"     # Linux/Mac

# Verify GPU is detected
python -c "from vhsdecode.gpu_utils import log_gpu_status; log_gpu_status()"
```

### Decoder Errors

**"Permission denied" on input file:**
- File may be locked by another process
- Close any programs using the file
- Try different sample file

**"Out of memory" errors:**
- Reduce thread count: `--threads 2`
- Process fewer frames at once
- Close other GPU applications
- Monitor GPU memory: `nvidia-smi`

**Slow GPU performance:**
- Ensure latest bug fixes applied (Dec 15, 2025+)
- Use 1-4 threads with GPU (not 8+)
- Check GPU isn't thermal throttling
- Verify SSD read speeds adequate

## Output Files

After successful decode:

```
output.tbc           # Video data (Time Base Corrected)
output.tbc.json      # Metadata (frame info, dropouts, etc.)
output_chroma.tbc    # Chroma (color) data
```

Convert to viewable format with `ld-chroma-decoder` and `ffmpeg` (see main README).

## Known Issues & Limitations (Dec 2025)

### GPU Status
- ✅ FFT operations fully on GPU
- ✅ Hilbert transform on GPU
- ✅ Phase unwrapping on GPU
- ⚠️ Envelope filtering uses CPU (IIR filter limitation)
- ⚠️ Some complex filters (VideoEQ, ChromaTrap) on CPU

### Bug Fixes Applied (Dec 15, 2025)
- Fixed: Envelope filtering shape mismatch errors
- Fixed: CuPy ediff1d type errors
- Result: Clean GPU operation with ~37% performance improvement

## Further Reading

- **Full Documentation:** [README.md](../README.md)
- **GPU Details:** [docs/GPU_USAGE.md](GPU_USAGE.md)
- **Bug Analysis:** [GPU_BUG_FIX_SUMMARY.md](../GPU_BUG_FIX_SUMMARY.md)
- **Performance:** [GPU_PERFORMANCE_ANALYSIS.md](../GPU_PERFORMANCE_ANALYSIS.md)

## Support

- **Issues:** https://github.com/oyvindln/vhs-decode/issues
- **Discord:** VHS Preservation Discord (see README)
- **Wiki:** Project wiki for detailed guides
