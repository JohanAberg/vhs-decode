---
description: Comprehensive AI coding instructions for VHS-Decode RF signal decoder
applyTo: '**'
priority: high
---

# VHS-Decode AI Coding Instructions

## Project Overview

VHS-Decode is a sophisticated RF signal decoder for analog video formats (VHS, LaserDisc, Betamax, Video8/Hi8, etc.). It processes raw 40 MSPS RF captures from tape heads directly, bypassing hardware video circuits through pure software decoding using FFT-based digital signal processing.

### Core Architecture: Block-Based FFT Pipeline

**Data Flow:**
```
Raw RF (40 MSPS) → 32KB FFT Blocks → RF Bandpass → Hilbert → FM Demod → Video/Chroma Separation → TBC Output
```

The decoder operates on fixed-size blocks (32K-131K samples) with overlap-save FFT processing, enabling parallel processing and filter operations in frequency domain. Key classes:

- **`VHSRFDecode`** (vhsdecode/process.py): Core decoder, inherits from lddecode.RFDecode
- **`VHSDecode`** (vhsdecode/process.py): High-level wrapper, handles formats/parameters  
- **`DemodCache`** (lddecode/core.py): Threaded block processing with caching

## Critical Development Principles

## Core Principles

### 1. **Test-First Development**
- Write tests BEFORE implementing GPU code
- Every GPU function must have a corresponding test
- Tests must compare GPU output against CPU baseline
- Never commit code that breaks existing tests

### 2. **Continuous Benchmarking**
- Measure performance at every step
- Track speedup relative to CPU baseline
- Document performance regressions immediately
- Update benchmark tracking with each commit

### 3. **Maintain CPU Compatibility**
- GPU code must fallback gracefully to CPU
- Support users without GPU hardware
- Test both code paths regularly
- Never remove CPU implementation

## Implementation Guidelines

### When Adding GPU-Accelerated Functions

1. **Verify CPU baseline exists and works**
   ```python
   # Ensure this passes first
   def test_cpu_baseline():
       result = cpu_function(test_data)
       assert validate(result)
   ```

2. **Write GPU test against CPU baseline**
   ```python
   def test_gpu_matches_cpu():
       cpu_result = cpu_function(test_data)
       gpu_result = gpu_function(test_data)
       np.testing.assert_allclose(cpu_result, gpu_result, rtol=1e-6)
   ```

3. **Implement GPU version**
   ```python
   def gpu_function(data):
       if not GPU_AVAILABLE:
           return cpu_function(data)
       # GPU implementation
   ```

4. **Benchmark performance**
   ```python
   def test_gpu_speedup():
       cpu_time = benchmark(cpu_function, test_data)
       gpu_time = benchmark(gpu_function, test_data)
       speedup = cpu_time / gpu_time
       assert speedup >= MINIMUM_SPEEDUP_TARGET
   ```

5. **Document results**
   - Update performance tracking
   - Note any precision trade-offs
   - Document memory requirements

### Code Organization

**File Structure:**
```
vhsdecode/
├── gpu_utils.py          # GPU detection, array module selection
├── process_gpu.py        # GPU-accelerated decoder classes
├── cuda_kernels/         # Custom CUDA kernels (Phase 2)
│   ├── __init__.py
│   └── phase_unwrap.py
└── process.py            # Original CPU implementation (KEEP)

tests/
├── fixtures/
│   └── gpu_validation/   # Test captures
├── test_gpu_unit.py      # Function-level tests
├── test_gpu_integration.py  # Pipeline tests
├── test_gpu_regression.py   # Output validation
├── test_gpu_benchmark.py    # Performance tests
└── test_gpu_memory.py       # Memory leak tests
```

### Naming Conventions

- GPU functions: `function_name_gpu()` or in `*_gpu.py` files
- CPU functions: Keep original names
- Unified interface: `function_name(..., use_gpu=False)`
- Test files: `test_gpu_*.py`

### Performance Targets

| Phase | Minimum Speedup | Goal Speedup | Status Check |
|-------|----------------|--------------|--------------|
| Phase 1 (CuPy) | 2.0x | 3.0x | Every commit |
| Phase 2 (CUDA) | 4.0x | 6.0x | Every commit |
| Phase 3 (Polish) | 5.0x | 8.0x | Daily |

**If speedup drops below minimum:**
1. Run profiler to identify bottleneck
2. Check for CPU/GPU transfer overhead
3. Verify batch sizes are optimal
4. Consider reverting if no fix found

### Testing Requirements

**Every Commit Must Include:**

1. **Unit Tests** - Test individual functions
   ```python
   @pytest.mark.gpu
   def test_gpu_fft():
       """Test GPU FFT matches CPU FFT"""
       # Test implementation
   ```

2. **Integration Tests** - Test full pipeline
   ```python
   @pytest.mark.gpu
   @pytest.mark.slow
   def test_full_decode_gpu():
       """Test complete decode pipeline"""
       # Test implementation
   ```

3. **Regression Tests** - Validate output quality
   ```python
   @pytest.mark.gpu
   def test_output_quality():
       """Ensure GPU output matches reference"""
       # Compare with known-good output
   ```

4. **Performance Tests** - Track speedup
   ```python
   @pytest.mark.benchmark
   def test_speedup(benchmark):
       """Measure and track performance"""
       # Benchmark implementation
   ```

**Test Data:**
- Use test captures from `tests/fixtures/gpu_validation/`
- If adding new test cases, document source and format
- Include reference outputs for regression testing
- Keep test files small (10-30 seconds) for fast CI

### Precision Guidelines

**Critical Operations (Use FP64):**
- Phase unwrapping
- Frequency-domain filtering with narrow bands
- Chroma heterodyne operations
- Dropout detection thresholds

**Safe for FP32:**
- General FFT operations
- Wide-band filtering
- Video level scaling
- Display operations

**Validation:**
```python
def test_precision():
    """Verify FP32 doesn't degrade output quality"""
    fp64_output = decode_with_precision(data, dtype=np.float64)
    fp32_output = decode_with_precision(data, dtype=np.float32)
    
    # Check SNR difference
    snr_diff = compute_snr(fp64_output) - compute_snr(fp32_output)
    assert abs(snr_diff) < 0.5, "FP32 degrades SNR"
```

### Memory Management

**Rules:**
1. Always check VRAM before allocation
2. Free memory explicitly when done
3. Use memory pools for repeated allocations
4. Test with 4GB GPU as minimum target

**Example:**
```python
def process_block_gpu(data):
    try:
        # Allocate on GPU
        gpu_data = cp.asarray(data)
        
        # Process
        result = process(gpu_data)
        
        # Transfer back
        return cp.asnumpy(result)
    finally:
        # Explicit cleanup
        del gpu_data
        cp.get_default_memory_pool().free_all_blocks()
```

### Error Handling

**Always Provide Fallback:**
```python
def decode_block(data, use_gpu=True):
    """Decode block with automatic fallback"""
    if use_gpu and GPU_AVAILABLE:
        try:
            return decode_block_gpu(data)
        except (cp.cuda.memory.OutOfMemoryError, GPUError) as e:
            logger.warning(f"GPU failed: {e}, falling back to CPU")
            use_gpu = False
    
    return decode_block_cpu(data)
```

### Documentation Requirements

**Every GPU Function Must Document:**
1. What it does
2. CPU equivalent function
3. Expected speedup
4. Memory requirements
5. Precision considerations
6. Test coverage

**Example:**
```python
def demodblock_gpu(self, data):
    """
    GPU-accelerated RF demodulation block.
    
    CPU Equivalent: demodblock() in process.py
    Expected Speedup: 3-4x (Phase 1), 5-7x (Phase 2)
    Memory Required: ~2-4 MB VRAM per block
    Precision: FP32 for FFT, FP64 for phase unwrapping
    
    Tests:
        - test_gpu_unit.py::test_demodblock_matches_cpu
        - test_gpu_integration.py::test_full_decode
        - test_gpu_benchmark.py::test_demodblock_speedup
    
    Args:
        data: Input RF data block (np.ndarray or cp.ndarray)
    
    Returns:
        dict: Demodulated video and chroma data
        
    Raises:
        GPUError: If GPU processing fails, falls back to CPU
    """
```

### Continuous Integration

**Pre-Commit Checks:**
```bash
# Run before every commit
pytest tests/test_gpu_unit.py -v        # Fast unit tests
pytest tests/test_gpu_regression.py -v  # Output validation
```

**Pre-Push Checks:**
```bash
# Run before pushing
pytest tests/test_gpu_integration.py -v  # Full pipeline tests
pytest tests/test_gpu_benchmark.py -v    # Performance validation
```

**CI Pipeline Must:**
1. Run all tests on GPU-enabled runner
2. Track benchmark results
3. Compare with previous commit
4. Alert on performance regression >10%
5. Block merge if tests fail

### Performance Debugging

**If Performance is Below Target:**

1. **Profile the code**
   ```python
   import cupy as cp
   with cp.cuda.profile():
       result = gpu_function(data)
   # Use nvprof or Nsight for detailed analysis
   ```

2. **Check transfer overhead**
   ```python
   # Time CPU→GPU transfer
   start = time.time()
   gpu_data = cp.asarray(cpu_data)
   transfer_time = time.time() - start
   
   # If transfer > compute, batch more operations on GPU
   ```

3. **Verify batch sizes**
   ```python
   # Larger blocks amortize overhead
   # Try 64K, 128K, 256K blocks
   # Monitor VRAM usage
   ```

4. **Check for CPU/GPU sync points**
   ```python
   # Avoid unnecessary synchronization
   # BAD: result = cp.asnumpy(gpu_data) in loop
   # GOOD: Keep on GPU until end
   ```

### Common Pitfalls to Avoid

❌ **DON'T:**
- Remove CPU implementation
- Commit untested code
- Skip benchmarking
- Ignore test failures
- Use FP32 everywhere without validation
- Transfer data unnecessarily between CPU/GPU
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
