---
description: Comprehensive AI coding instructions for VHS-Decode RF signal decoder
applyTo: '**'
priority: high
---

# VHS-Decode AI Guide

## Quick Start (Most Common Tasks)

### 1. Running a Decode
```bash
# Always activate environment first
.\.venv\Scripts\Activate.ps1

# GPU decode (recommended - 34% faster than CPU)
python decode.py vhs --system PAL --gpu --threads 1 input.u8 output

# CPU decode (8 threads baseline)
python decode.py vhs --system PAL --threads 8 input.u8 output

# Quick test (11 frames)
python decode.py vhs --system PAL --gpu --length 11 sample_data/out2.u8 test
```

### 2. Profiling GPU Performance
```bash
.\.venv\Scripts\Activate.ps1
python decode.py vhs --system PAL --gpu --gpu-profile --length 50 input.u8 output
# Review GPU_OPTIMIZATION_ANALYSIS.md for bottleneck analysis
```

### 3. After Code Changes
```bash
.\.venv\Scripts\Activate.ps1
pip install -e . --no-deps
python decode.py vhs --gpu --length 11 sample_data/out2.u8 test  # Verify it works
```

### 4. Running Tests
```bash
.\.venv\Scripts\Activate.ps1
pytest tests/test_gpu_unit.py -v                    # Quick GPU tests
pytest tests/ -v                                     # Full test suite
```

## Pipeline + Architecture
- Raw 40 MSPS RF goes through 32K-131K overlap-save FFT blocks: RF bandpass → Hilbert → FM demod → video/chroma split → TBC writer (see vhsdecode/process.py).
- `VHSRFDecode` handles per-block DSP; `VHSDecode` wraps system/tape presets; `DemodCache` in lddecode/core.py manages threaded block queues.
- Filters originate from compute_video_filters.py CSVs; keep block sizes consistent so frequency-domain kernels stay aligned.
- CX, DdD, and MISRC captures are assumed to be lossless; any preprocessing belongs in lddecode/utils_* rather than mutating RF blocks.

## Key Modules & Patterns
- CPU path lives in vhsdecode/process.py; GPU mirror resides in vhsdecode/process_gpu.py with helpers in vhsdecode/gpu_utils.py and cuda_kernels/phase_unwrap.py.
- Shared math helpers are NUMPY-first; GPU code must accept both NumPy and CuPy arrays and free GPU pools after use.
- CLI entry point decode.py dispatches to VHS/CVBS/HiFi decoders; keep option parsing/validation there and avoid duplicating flag logic inside decoders.
- Benchmarks and regression metadata are stored under tests/benchmark_tracker.py and benchmark_results.json—update when changing performance-critical sections.

## GPU Workflow (Phase 2 status Dec 15 2025 - GPU ACTIVATION COMPLETE)
- **GPU fully activated:** 34% performance improvement over CPU (2.49 FPS GPU vs 2.19 FPS CPU/8-thread on RTX 4070 Ti)
- All RF blocks processed through Phase 2 optimized GPU path with zero GPU idle time
- **Key fixes applied (Dec 15 2025):**
  - Implemented AsyncLoader for background RF block prefetching (prevents GPU starvation)
  - Added `demodblock()` override to VHSRFDecodeGPU class (was inheriting CPU path)
  - Fixed infinite recursion in `_demodblock_single()` routing method
  - Implemented lazy filter initialization (defers scipy IIR→FIR conversion to first block)
  - Fixed Python bytecode cache issues preventing GPU code path activation
- Always offer CPU fallback (`use_gpu` flag, try/except cp.cuda.memory.OutOfMemoryError) and log degradations rather than raising.
- Precision: keep FFT/filter work in FP32, but phase unwrap/chroma heterodyne/dropout logic stay FP64; assert tolerances at rtol=1e-4/atol=1e-4.
- Document every GPU entry point with CPU equivalent, expected speedup, memory footprint, and linked tests (see GPU_PERFORMANCE_ANALYSIS.md and docs/GPU_USAGE.md).
- **Known GPU limitations:** FEnvPost (IIR/SOS filter) runs on CPU - this is correct and documented, not a bug.

### GPU Profiling (Dec 15 2025 - Verified Working)
- **Enable profiling:** Add `--gpu-profile` flag to any GPU decode command
- **Output:** Detailed timing breakdown showing % time per operation + memory transfer stats
- **Profiler class:** `GPUProfiler` in vhsdecode/gpu_utils.py tracks all GPU operations
- **Current profile (50 frames on RTX 4070 Ti):**
  - Envelope Filter GPU: 11.9% (1648ms) - distributed across operations
  - Chroma Processing: 11.0% (1524ms)
  - Envelope Calculation: 10.5% (1459ms)
  - Data Transfer CPU→GPU: 10.5% (1450ms)
  - Data Transfer GPU→CPU: 10.4% (1449ms)
  - No single bottleneck: well-distributed pipeline (5-12% each)
- **Memory profile:** 200MB CPU→GPU, 2400MB GPU→CPU (12x asymmetry expected), 16.83MB peak GPU memory
- **Documentation:** See GPU_OPTIMIZATION_ANALYSIS.md and GPU_PROFILING_GUIDE.md for details
- **Usage pattern:**
  ```bash
  .\.venv\Scripts\Activate.ps1
  python decode.py vhs --gpu --gpu-profile --length 50 --system PAL input.u8 output
  # Prints profiling summary at end with operation-level timing breakdown
  ```

## Tests, Fixtures, Benchmarks
- Quick validation: `pytest tests/test_gpu_unit.py -v` and `pytest tests/test_gpu_regression.py -v`; full pipeline/perf: `pytest tests/test_gpu_integration.py tests/test_gpu_benchmark.py -v`.
- Test captures live in tests/fixtures/gpu_validation/ alongside reference `.tbc/.json`; never commit new samples without updating tests/fixtures/README.md.
- Use BenchmarkTracker (tests/benchmark_tracker.py) to record cpu_time/gpu_time pairs after changes to demod blocks or GPU kernels; keep scaling_benchmark_results.json current.
- Performance failures (<2x speedup for FFT/chroma blocks) require profiling with `cp.cuda.profile()` or Nsight before merging.

## Build + CLI Usage

### Python Environment Setup (Windows)
- **Virtual Environment:** Project uses `.venv` located at workspace root
- **Activation (PowerShell):**
  ```powershell
  .\.venv\Scripts\Activate.ps1
  ```
- **Activation (CMD):**
  ```cmd
  .venv\Scripts\activate.bat
  ```
- **Check Active:** Prompt shows `(.venv)` prefix when activated
- **ALWAYS activate .venv before running any Python commands** (decode.py, tests, profiling, etc.)

### Installation
- Install via `pip install -e .` in dev mode; add CuPy for GPU: `pip install cupy-cuda12x` (CUDA 12) or `cupy-cuda11x` (CUDA 11).
- After package changes, reinstall: `pip install -e . --no-deps` to pick up code changes.
- Use LD/CVBS/HiFi sibling tools from the same repo; changes to shared lddecode modules must pass both VHS and LD pipelines.

### Running Decodes
- Decode command pattern: `python decode.py vhs input.r40 output --system PAL --threads 8` (CPU) or `--gpu --threads 1` (GPU).
- GPU requires: NVIDIA GPU with CUDA 11+, CuPy installed, and 1 thread optimal (GPU thread overhead not beneficial for RF processing).
- Example commands:
  ```bash
  # Activate environment first (REQUIRED)
  .\.venv\Scripts\Activate.ps1
  
  # CPU decode (8 threads - 2.19 FPS baseline)
  python decode.py vhs --system PAL --threads 8 input.r40 output
  
  # GPU decode (1 thread - 2.49 FPS, 34% faster than CPU)
  python decode.py vhs --system PAL --gpu --threads 1 input.r40 output
  
  # Quick test (11 frames)
  python decode.py vhs --system PAL --gpu --length 11 input.r40 test
  
  # GPU with profiling (for optimization work)
  python decode.py vhs --system PAL --gpu --gpu-profile --length 50 input.u8 output
  ```

## Data & Debugging Tips
- Reference captures for README/test screenshots are under assets/ and tests/fixtures; update both when adjusting decoding defaults.
- When tracing weird output, enable `--debug-plot` hooks (plots live in lddecode/utils_plotting.py) instead of sprinkling temporary prints.
- GPU memory issues usually stem from forgotten `cp.get_default_memory_pool().free_all_blocks()` calls—mirror cleanup patterns already present in process_gpu.py.
- Regression artifacts are easiest to spot by comparing generated `.tbc` with output_cpu.tbc/output_gpu.tbc already in the repository; keep those up to date with golden results.
- Allocate memory in loops
- Forget to free GPU memory

✅ **DO:**
- Keep CPU and GPU paths
- Test every function
- Benchmark every change
- Fix failing tests immediately
- Validate precision requirements
- Batch operations on GPU
- Reuse memory allocations
- Profile performance regularly

### Review Checklist

Before submitting GPU code, verify:

- [ ] All existing tests still pass
- [ ] New tests added for GPU functionality
- [ ] GPU output matches CPU within tolerance
- [ ] Performance meets or exceeds target
- [ ] Benchmark results recorded
- [ ] Fallback to CPU works correctly
- [ ] Memory is freed properly
- [ ] Documentation updated
- [ ] Error handling is robust
- [ ] Code follows project style

### Getting Help

**If You're Stuck:**
1. Check existing test cases for examples
2. Review GPU_IMPLEMENTATION_PROPOSAL.md
3. Profile to identify bottleneck
4. Test on smaller data first
5. Ask in project issues with:
   - What you're trying to do
   - Test results (pass/fail)
   - Benchmark numbers
   - Error messages

### Performance Tracking

**Use the Benchmark Tracker:**
```python
from tests.benchmark_tracker import BenchmarkTracker

tracker = BenchmarkTracker()
tracker.record(
    test_name="demodblock_32k",
    cpu_time=0.850,
    gpu_time=0.280,
    commit_hash=get_git_commit()
)
```

**Generate Performance Report:**
```bash
python tests/generate_performance_report.py
# Creates benchmark_report.md with speedup trends
```

### Test Data Management

**Required Test Captures:**
- VHS NTSC color bars (10s)
- VHS PAL test card (10s)
- SVHS NTSC hi-fi (10s)
- VHS with dropouts (10s)

**Location:** `tests/fixtures/gpu_validation/`

**If Test Data Missing:**
1. Use community samples from vhs-decode Discord
2. Generate using existing captures
3. Document source in `tests/fixtures/README.md`

**Reference Outputs:**
- Generate with current CPU decoder
- Store as `.tbc` and `.tbc.json`
- Update when CPU decoder changes significantly

## Project Architecture & Development Workflows

### Block Processing Pipeline

Understand the core data flow through the decoder:

1. **Raw RF Input** (40 MSPS samples)
2. **DemodCache** manages threaded block processing
3. **VHSRFDecode.demodblock()** - Core per-block processing:
   - FFT → RF filtering → Hilbert transform → FM demod → Video/Chroma separation
4. **Field assembly** and time-base correction
5. **TBC output** (.tbc/.json files)

**Key Files:**
- [lddecode/core.py](lddecode/core.py) - Base RFDecode class, DemodCache threading
- [vhsdecode/process.py](vhsdecode/process.py) - VHS-specific decoder implementation
- [vhsdecode/formats.py](vhsdecode/formats.py) - Format definitions and parameters

### Signal Processing Patterns

**FFT Operations:** Most processing uses overlap-save FFT with frequency-domain filtering:
```python
# Standard pattern throughout codebase
indata_fft = npfft.fft(data[:self.blocklen])
filtered_fft = indata_fft * self.Filters["FilterName"]
output = npfft.ifft(filtered_fft).real
```

**Filter Definitions:** Located in `compute_video_filters.py` - precomputed frequency-domain filters

### Testing & Quality Assurance

**Run Tests:**
```bash
# Quick validation
python -m pytest tests/test_gpu_quick.py -v

# Full test suite (requires test captures)
python -m pytest tests/ -v --tb=short

# Performance benchmarking
python tests/test_gpu_benchmark.py
```

**Test Data:** Store validation captures in `tests/fixtures/gpu_validation/`

### Build System

**Python Development:**
```bash
# Install in development mode
pip install -e .

# With GPU support
pip install -e .[gpu]

# Run decoder
vhs-decode input.r40 output.tbc --system NTSC
```

**C++ Tools (CMake):**
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Format Support

**Tape Formats:** VHS, SVHS, Betamax, Video8/Hi8, U-Matic, etc.
**TV Systems:** NTSC, PAL, PAL-M, SECAM/MESECAM, 405-line
**Sample Rates:** Typically 40 MSPS, but configurable

**Format Selection:**
```bash
vhs-decode input.r40 output.tbc --system PAL --tape-format SVHS
```

### Key Dependencies

**Core:** NumPy, SciPy, Numba for performance-critical code
**Optional:** CuPy for GPU acceleration, PyQt5/6 for GUI tools
**Build:** CMake for C++ tools, setuptools_scm for versioning

### Cross-Platform Considerations

**Platform Detection:** [vhsdecode/__init__.py](vhsdecode/__init__.py) handles platform-specific library loading
**Build Differences:** Windows uses different library paths, macOS requires special thread handling

### Performance Optimization

**CPU Optimization:**
- Numba JIT compilation for hot paths (`.pyx` files)
- Vectorized NumPy operations
- Pre-computed filter coefficients

**Memory Management:**
- Fixed block sizes to avoid allocation overhead
- Overlap-save processing to reuse FFT computations
- LRU cache in DemodCache for processed blocks

### Debugging Workflows

**Debug Plots:** Use `--debug-plot` flag to visualize signal processing stages

**Common Issues:**
- **Poor tracking:** Check RF signal strength in envelope detector
- **Sync issues:** Verify hsync detection with 0.5MHz filter
- **Dropouts:** Check envelope detection thresholds

**Log Analysis:**
```python
import logging
logging.getLogger('lddecode').setLevel(logging.DEBUG)
```

## Recent GPU Bug Fixes (Dec 15, 2025)

### Fixed Bug #1: Envelope Filtering Shape Mismatch
- **Error:** "GPU envelope filtering failed: Out shape is mismatched" (400+ occurrences)
- **Cause:** FEnvPost is IIR/SOS filter, cannot be used as frequency-domain FFT filter
- **Fix:** Removed broken GPU path in process_gpu.py line 462-472, use CPU directly
- **Result:** Clean decode, 37% faster than buggy version

### Fixed Bug #2: CuPy ediff1d Type Error  
- **Error:** "Unexpected error: 'to_begin' should be of type cupy.ndarray"
- **Cause:** CuPy requires `to_begin=cp.array([0])` not `to_begin=0`
- **Fix:** Changed process_gpu.py line 512 to use CuPy array type
- **Result:** GPU spike replacement working correctly

### Important IIR Filter Pattern
```python
# WRONG - Don't use IIR/SOS filters in frequency domain
env_fft = cp.fft.rfft(data)
filtered = env_fft * sos_filter  # ERROR: shape mismatch

# CORRECT - IIR filters must be applied in time domain
from scipy import signal
filtered = signal.sosfiltfilt(sos_filter, data)  # CPU only for now
```

## Summary

**Three Golden Rules for GPU Development:**
1. ✅ **Test Everything** - Write tests first, validate against CPU
2. 📊 **Benchmark Constantly** - Track performance at every step
3. 🔄 **Maintain Compatibility** - Always support CPU fallback

**Critical GPU Knowledge:**
- IIR/SOS filters (like FEnvPost) cannot be used with FFT multiplication - use CPU `sosfiltfilt`
- CuPy parameter types must match (arrays for array params, not Python scalars)
- Always test with: `.\.venv\Scripts\Activate.ps1; python decode.py vhs --gpu --length 11 sample_data/out2.u8 test` - should show zero errors
- After code changes: `pip install -e . --no-deps` then test immediately
- **Always activate .venv first** before any Python operations

**Architecture Understanding:**
- Master the block-based FFT pipeline
- Understand DemodCache threading model
- Know the signal processing flow from RF to TBC
- Know which filters are FIR (FFT-compatible) vs IIR (time-domain only)

**GPU Profiling & Optimization:**
- Use `--gpu-profile` flag to identify bottlenecks before optimizing
- Current targets: FM demod (46%), Envelope filter (22%), Memory transfers (19%)
- Expected gains: 2.8-4x speedup possible with targeted optimization
- See GPU_OPTIMIZATION_ANALYSIS.md for detailed action plan

**Documentation:**
- Usage guide: [docs/DECODER_USAGE.md](docs/DECODER_USAGE.md)
- GPU setup: [docs/GPU_USAGE.md](docs/GPU_USAGE.md)
- Bug fixes: [GPU_BUG_FIX_SUMMARY.md](GPU_BUG_FIX_SUMMARY.md)
- **Profiling guide: [GPU_PROFILING_GUIDE.md](GPU_PROFILING_GUIDE.md)** ← Use this for optimization work
- **Optimization targets: [GPU_OPTIMIZATION_ANALYSIS.md](GPU_OPTIMIZATION_ANALYSIS.md)** ← Roadmap for speedups
- **C++ Prototype status: [cpp-prototype/CPP_PROTOTYPE_STATUS.md](cpp-prototype/CPP_PROTOTYPE_STATUS.md)** ← C++ implementation details

Following these guidelines ensures both GPU acceleration and general development are correct, fast, and maintainable.

## Documentation Maintenance Rules

**ALWAYS keep documentation files up to date when making changes:**
1. **After modifying C++ prototype:** Update `cpp-prototype/CPP_PROTOTYPE_STATUS.md`
2. **After modifying Python GPU code:** Update `GPU_PERFORMANCE_ANALYSIS.md` and `docs/GPU_USAGE.md`
3. **After fixing bugs:** Document in relevant `*_BUG_FIX_SUMMARY.md` files
4. **After adding features:** Update this file (`copilot-instructions.md`) with new parameters/patterns
5. **After performance changes:** Update benchmark results in `scaling_benchmark_results.json`

**Key documentation files to maintain:**
- `.github/copilot-instructions.md` - This file, master AI guide
- `cpp-prototype/CPP_PROTOTYPE_STATUS.md` - C++ implementation status and comparison with Python
- `docs/DECODER_USAGE.md` - User-facing decoder documentation
- `docs/GPU_USAGE.md` - GPU-specific usage guide

## C++ Prototype (Updated Dec 21, 2025)

The `cpp-prototype/` directory contains a C++ implementation of the VHS RF decoder,
designed for GPU acceleration via OpenCL.

### Quick Start - C++ Prototype

```bash
# Build
cd cpp-prototype
mkdir -p build && cd build
cmake ..
cmake --build . --parallel 8

# Run decode
./src/vhs-decode --system PAL --length 10 input.u8 output

# Run decode aligned with Python (use --seek with Python's first field fileLoc)
./src/vhs-decode --system PAL --length 3 --seek 983040 input.u8 output
```

### Working Components
- **RF Reader:** Memory-mapped file I/O for efficient large file handling
- **FFT Engine:** FFTW3 (single-precision) for FFT/iFFT
- **Filter Bank:** Frequency-domain bandpass/lowpass/highpass filters
- **Hilbert Transform:** Analytic signal generation
- **FM Demodulator:** Phase extraction and frequency scaling
- **TBC Scaler:** Bicubic interpolation resampling 2560→1135 samples/line
- **Horizontal Sync Detection:** Line sync pulse detection
- **Vertical Sync Detection:** Field boundary detection via state machine
- **TBC Writer:** .tbc and .tbc.json output at 17.734475 MHz (4×fsc)

### VHS PAL Parameters (CORRECTED - from vhsdecode/format_defs/vhs.py)
```cpp
// RF Bandpass: 1.3 - 5.78 MHz
rfConfig.rfBandpassLowMHz = 1.3;
rfConfig.rfBandpassHighMHz = 5.78;

// Frequency to IRE mapping (CORRECT values from Python)
float ire0Hz = 4100000.0f;       // 0 IRE (black level) - 4.1 MHz
float hzPerIre = 7000.0f;        // Hz per IRE
float vsyncIre = -42.857f;       // Sync tip in IRE (PAL standard)
// Sync tip (-42.857 IRE) at 3.8 MHz
// Peak white (100 IRE) at 4.8 MHz

// Digital scaling (from lddecode/core.py FieldPAL)
float outputZero = 256.0f;       // Digital value at sync tip
float outScale = 53760.0f / 142.857f;  // = 376.32
// Formula: output = (ire - vsync_ire) * out_scale + outputZero
// -42.857 IRE → 256, 0 IRE → 16384, 100 IRE → 54016

// TBC output format
size_t outputLineLen = 1135;     // 4 × 4.43361875 MHz × 64µs
double outputSampleRate = 17734475.0;  // 4×fsc for PAL
```

### C++ vs Python Comparison (Dec 21, 2025)
| Metric | Python | C++ | Status |
|--------|--------|-----|--------|
| Output line length | 1135 | 1135 | ✓ Match |
| Output sample rate | 17.734475 MHz | 17.734475 MHz | ✓ Match |
| Mean digital value | ~19857 | ~19196 | Close |
| Field length | ~312 lines | ~312 lines | ✓ Match |
| Vsync detection | - | ✓ Working | New - 4 fields found |

**Vsync detector results (out1.u8):**
- Detected 1718 pulses: HSYNC=1251, EQ=42, VSYNC=21
- Found 4 field boundaries
- First field at sample 191190 (line ~74)
- Field length: 798735 samples (~312 lines)

### Missing Components (TODO - Priority Order)
1. **Sub-sample line positioning** - Python uses linelocs interpolation
2. **Signal enhancement** - High-frequency boost, spike replacement
3. **Dropout detection** - Not implemented
4. **Chroma processing** - Currently copies luma

### Key Files
- `cpp-prototype/src/core/main.cpp` - Entry point with --seek option
- `cpp-prototype/src/rf/rf_processor.cpp` - RF pipeline
- `cpp-prototype/src/demod/fm_demodulator.cpp` - FM demodulation
- `cpp-prototype/src/tbc/tbc_scaler.cpp` - TBC resampling
- `cpp-prototype/src/sync/sync_detector.cpp` - Horizontal sync detection
- `cpp-prototype/src/sync/vsync_detector.cpp` - Vertical sync detection (NEW)
- `cpp-prototype/CPP_PROTOTYPE_STATUS.md` - Detailed status and comparison

### Testing C++ Output
```bash
# Decode with Python first
python decode.py vhs --system PAL --length 3 input.u8 python_output

# Get Python's starting position
python -c "import json; print(json.load(open('python_output.tbc.json'))['fields'][0]['fileLoc'])"

# Decode with C++ at same position
./src/vhs-decode --system PAL --length 3 --seek <fileLoc> input.u8 cpp_output

# Compare correlation
python3 << 'EOF'
import numpy as np
py = np.fromfile('python_output.tbc', dtype=np.uint16)[:1135*312]
cpp = np.fromfile('cpp_output.tbc', dtype=np.uint16)[:1135*312]
corr = np.corrcoef(py, cpp)[0,1]
print(f"Correlation: {corr:.4f}")
EOF
```

### C++ Development Guidelines
- Use FFTW3f (single-precision) for main processing
- Match Python filter parameters from `vhsdecode/format_defs/vhs.py`
- Keep CPU fallback for all GPU operations
- Test against Python decoder output for validation
- **Always update CPP_PROTOTYPE_STATUS.md after changes**

