---
description: Comprehensive AI coding instructions for VHS-Decode RF signal decoder
applyTo: '**'
priority: high
---

# VHS-Decode AI Guide

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

## GPU Workflow (Phase 1 status Dec 15 2025)
- Framework is correct but 13 % slower than CPU because of 17 CPU↔GPU transfers; batch multiple operations per transfer and prefer 4 GPU threads vs 8 CPU threads.
- Always offer CPU fallback (`use_gpu` flag, try/except cp.cuda.memory.OutOfMemoryError) and log degradations rather than raising.
- Precision: keep FFT/filter work in FP32, but phase unwrap/chroma heterodyne/dropout logic stay FP64; assert tolerances at rtol=1e-4/atol=1e-4.
- Document every GPU entry point with CPU equivalent, expected speedup, memory footprint, and linked tests (see GPU_PERFORMANCE_ANALYSIS.md and docs/GPU_USAGE.md).

## Tests, Fixtures, Benchmarks
- Quick validation: `pytest tests/test_gpu_unit.py -v` and `pytest tests/test_gpu_regression.py -v`; full pipeline/perf: `pytest tests/test_gpu_integration.py tests/test_gpu_benchmark.py -v`.
- Test captures live in tests/fixtures/gpu_validation/ alongside reference `.tbc/.json`; never commit new samples without updating tests/fixtures/README.md.
- Use BenchmarkTracker (tests/benchmark_tracker.py) to record cpu_time/gpu_time pairs after changes to demod blocks or GPU kernels; keep scaling_benchmark_results.json current.
- Performance failures (<2x speedup for FFT/chroma blocks) require profiling with `cp.cuda.profile()` or Nsight before merging.

## Build + CLI Usage
- Install via `pipx install .` (add `[hifi_gui_qt6]` for GUI or `[gpu]` after CUDA/CuPy setup); Windows portable builds live under build/lib.*.
- Decode command pattern: `vhs-decode input.r40 output_dir --system PAL --threads 8` (CPU) or `--gpu --threads 4` (GPU); keep samples on SSD to avoid I/O skew.
- Use LD/CVBS/HiFi sibling tools from the same repo; changes to shared lddecode modules must pass both VHS and LD pipelines.

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

## Summary

**Three Golden Rules for GPU Development:**
1. ✅ **Test Everything** - Write tests first, validate against CPU
2. 📊 **Benchmark Constantly** - Track performance at every step
3. 🔄 **Maintain Compatibility** - Always support CPU fallback

**Architecture Understanding:**
- Master the block-based FFT pipeline
- Understand DemodCache threading model
- Know the signal processing flow from RF to TBC

Following these guidelines ensures both GPU acceleration and general development are correct, fast, and maintainable.
