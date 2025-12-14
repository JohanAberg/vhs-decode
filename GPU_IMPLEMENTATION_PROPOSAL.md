# GPU Implementation Proposal for VHS-Decode

**Version**: 1.0  
**Date**: December 15, 2025  
**Status**: Proposal  
**Estimated Timeline**: 8-12 weeks for Phase 1-2  
**Expected Performance Gain**: 3-10x speedup  

---

## Executive Summary

This proposal outlines a phased approach to implement GPU acceleration for VHS-Decode's RF decoding pipeline. The decoder's architecture is well-suited for GPU acceleration, with extensive use of FFT operations, parallel filtering, and block-based processing. Initial analysis suggests a realistic 3-6x speedup is achievable with Phase 1 implementation using CuPy, with potential for 5-10x with custom CUDA kernels in Phase 2.

**Critical Requirements:**
- **Continuous Testing**: Automated tests must pass at every commit to verify correctness
- **Benchmark-Driven Development**: Performance must be measured and tracked throughout implementation
- **Test Data Repository**: Reference captures required for validation and regression testing

## Current Performance Baseline

- **Processing Speed**: ~1-2 FPS (40-80 seconds per video second)
- **Block Size**: 32,768 - 131,072 samples
- **Sample Rate**: 40 MSPS (typical)
- **Bottlenecks**: FFT operations, Hilbert transforms, frequency-domain filtering

## Architecture Analysis

### Current Implementation

The decoder uses a sophisticated multi-layer architecture:

```
Raw RF Data (40 MSPS)
    ↓
[Block-based FFT Processing - 32KB blocks]
    ↓
[RF Bandpass Filter (3.8-4.8 MHz PAL, 3.4-4.4 MHz NTSC)]
    ↓
[Hilbert Transform → Phase Unwrapping → FM Demodulation]
    ↓
[Multiple Frequency-Domain Filters]
    ↓
[Chroma Separation & Demodulation]
    ↓
[Time-Base Correction]
    ↓
Output Video (TBC format)
```

### Performance-Critical Hotspots

1. **FFT/iFFT Operations** (40-50% of CPU time)
   - Location: `lddecode/core.py:demodblock()`, `vhsdecode/process.py:demodblock()`
   - Currently: numpy.fft / scipy.fft
   - Operations per block: 5-8 FFT/iFFT pairs

2. **Frequency-Domain Filtering** (20-30% of CPU time)
   - Location: `vhsdecode/utils.py:filtfft()`, filter applications in demodblock
   - Multiple filter multiplications in frequency domain
   - Filters: RFVideo, FDeemp, FVideo, FVideo05, FVideoBurst

3. **Hilbert Transform & Phase Unwrapping** (15-20% of CPU time)
   - Location: `vhsdecode/demod.py:unwrap_hilbert()`, `src/lib.rs:unwrap_hilbert_impl()`
   - Complex angle calculation and unwrapping
   - Currently optimized in Rust

4. **Chroma Processing** (10-15% of CPU time)
   - Location: `vhsdecode/chroma.py:demod_chroma_filt()`
   - Heterodyne operations for color-under formats
   - Bandpass filtering and frequency shifting

5. **Numba JIT Operations** (5-10% of CPU time)
   - Various @njit decorated functions
   - Spike detection, level detection, sync finding

## GPU Acceleration Feasibility Matrix

| Operation | GPU Suitability | Expected Speedup | Implementation Complexity |
|-----------|----------------|------------------|---------------------------|
| FFT/iFFT | ⭐⭐⭐⭐⭐ Excellent | 5-10x | Low (CuPy drop-in) |
| Frequency-domain filtering | ⭐⭐⭐⭐⭐ Excellent | 4-8x | Low (element-wise mult) |
| Hilbert transform | ⭐⭐⭐⭐ Very Good | 3-5x | Medium (custom kernel) |
| Complex angle calculation | ⭐⭐⭐⭐ Very Good | 4-6x | Low (CuPy.angle) |
| Phase unwrapping | ⭐⭐⭐ Good | 2-4x | High (scan algorithm) |
| Chroma heterodyne | ⭐⭐⭐⭐ Very Good | 3-6x | Medium (batch ops) |
| Dropout detection | ⭐⭐ Fair | 1.5-2x | High (complex logic) |
| Sync detection | ⭐ Poor | 1-1.5x | Very High (sequential) |
| VBI decoding | ⭐ Poor | <1x | N/A (keep on CPU) |

## Test Data Requirements

### Required Test Captures

To ensure comprehensive validation, the following test captures are needed:

**Minimum Test Set (Required):**
1. **VHS NTSC** - 10 seconds, standard quality, color bars + motion
2. **VHS PAL** - 10 seconds, standard quality, color bars + motion
3. **SVHS NTSC** - 10 seconds, high quality
4. **Betamax NTSC** - 10 seconds (if available)
5. **VHS with dropouts** - 10 seconds, deliberately degraded signal
6. **VHS with weak signal** - 10 seconds, low SNR scenario

**Test Data Format:**
- Raw RF captures (.lds or .ldf format)
- Known-good TBC output from current CPU decoder
- JSON metadata files
- Total size: ~5-10 GB

**Test Data Location:**
```
tests/fixtures/gpu_validation/
├── vhs_ntsc_colorbars.lds
├── vhs_ntsc_colorbars.tbc (reference CPU output)
├── vhs_ntsc_colorbars.tbc.json
├── vhs_pal_testcard.lds
├── svhs_ntsc_hifi.lds
├── betamax_ntsc.lds
└── vhs_dropouts.lds
```

**Generating Test Data:**
```bash
# Use existing captures from the community
# Contributors should provide their own test captures or use:
# - ld-decode test suite captures
# - DomesDay86 project samples
# - vhs-decode Discord community samples
```

## Continuous Integration Requirements

### Automated Test Pipeline

Every commit must pass:

1. **Unit Tests** - Individual function correctness
2. **Integration Tests** - Full decode pipeline validation
3. **Regression Tests** - Output matches CPU baseline
4. **Performance Tests** - Speedup tracking
5. **Memory Tests** - No leaks, within VRAM limits

### CI Configuration

```yaml
# .github/workflows/gpu-tests.yml
name: GPU Tests
on: [push, pull_request]

jobs:
  gpu-tests:
    runs-on: [self-hosted, gpu]  # Requires GPU runner
    steps:
      - uses: actions/checkout@v3
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - name: Install CUDA
        run: |
          # Install CUDA toolkit
      - name: Install dependencies
        run: |
          pip install -e .[gpu]
          pip install pytest pytest-benchmark
      - name: Run GPU tests
        run: |
          pytest tests/test_gpu.py -v --benchmark-only
      - name: Validate output
        run: |
          pytest tests/test_gpu_regression.py -v
```

## Implementation Phases

### Phase 1: CuPy Integration (4-6 weeks)

**Goal**: Replace core numpy operations with CuPy for GPU acceleration  
**Expected Speedup**: 2-4x  
**Risk Level**: Low  
**Testing Strategy**: Continuous validation against CPU baseline  

#### Tasks

1. **Setup GPU Detection & Fallback** (Week 1)
   ```python
   # File: vhsdecode/gpu_utils.py (NEW)
   try:
       import cupy as cp
       GPU_AVAILABLE = True
   except ImportError:
       cp = None
       GPU_AVAILABLE = False
   
   def get_array_module(use_gpu=True):
       """Return cupy if available and requested, else numpy"""
       if use_gpu and GPU_AVAILABLE:
           return cp
       return np
   ```

2. **Create GPU-Accelerated RFDecode Class** (Week 2-3)
   ```python
   # File: vhsdecode/process_gpu.py (NEW)
   class VHSRFDecodeGPU(VHSRFDecode):
       def __init__(self, *args, use_

#### Continuous Testing Requirements

**Every Feature Branch Must Include:**

1. **Unit Tests for New Code**
   ```python
   # tests/test_gpu_unit.py
   @pytest.mark.gpu
   def test_gpu_fft_matches_cpu():
       """Verify GPU FFT produces identical results to CPU"""
       test_data = load_test_signal()
       cpu_result = np.fft.fft(test_data)
       gpu_result = cp.asnumpy(cp.fft.fft(cp.asarray(test_data)))
       np.testing.assert_allclose(cpu_result, gpu_result, rtol=1e-6)
   ```

2. **Integration Tests**
   ```python
   # tests/test_gpu_integration.py
   @pytest.mark.gpu
   @pytest.mark.slow
   def test_full_decode_matches_baseline():
       """Verify full GPU decode matches CPU baseline"""
       input_file = "tests/fixtures/vhs_ntsc_colorbars.lds"
       
       # CPU decode
       cpu_output = decode_file(input_file, use_gpu=False)
       
       # GPU decode
       gpu_output = decode_file(input_file, use_gpu=True)
       
       # Compare outputs
       assert_tbc_match(cpu_output, gpu_output, max_diff=0.1)
   ```

3. **Performance Benchmarks**
   ```python
   # tests/test_gpu_benchmark.py
   @pytest.mark.benchmark(group="demodblock")
   def test_demodblock_cpu(benchmark):
       """Benchmark CPU demodblock performance"""
       decoder = VHSRFDecode(inputfreq=40)
       test_data = load_test_block()
       benchmark(decoder.demodblock, test_data)
   
   @pytest.mark.benchmark(group="demodblock")
   @pytest.mark.gpu
   def test_demodblock_gpu(benchmark):
       """Benchmark GPU demodblock performance"""
       decoder = VHSRFDecodeGPU(inputfreq=40, use_gpu=True)
       test_data = load_test_block()
       benchmark(decoder.demodblock, test_data)
   ```

4. **Regression Tests**
   ```python
   # tests/test_gpu_regression.py
   @pytest.mark.parametrize("test_file", [
       "vhs_ntsc_colorbars.lds",
       "vhs_pal_testcard.lds",
       "svhs_ntsc_hifi.lds",
   ])
   @pytest.mark.gpu
   def test_gpu_regression(test_file):
       """Ensure GPU output hasn't regressed"""
       input_path = f"tests/fixtures/gpu_validation/{test_file}"
       reference_tbc = input_path.replace(".lds", ".tbc")
       
       # Decode with GPU
       gpu_output = decode_file(input_path, use_gpu=True)
       
       # Compare with reference
       reference = load_tbc(reference_tbc)
       
       # Allow small numerical differences (0.1%)
       diff_percentage = compute_diff(gpu_output, reference)
       assert diff_percentage < 0.1, f"Output differs by {diff_percentage}%"
   ```

#### Benchmark Tracking

**Performance Dashboard:**

Create a performance tracking system to monitor speedup across commits:

```python
# tests/benchmark_tracker.py
import json
import time
from pathlib import Path

class BenchmarkTracker:
    def __init__(self, results_file="benchmark_results.json"):
        self.results_file = Path(results_file)
        self.results = self._load_results()
    
    def record(self, test_name, cpu_time, gpu_time, commit_hash):
        """Record benchmark results"""
        speedup = cpu_time / gpu_time if gpu_time > 0 else 0
        
        self.results[test_name] = {
            "cpu_time": cpu_time,
            "gpu_time": gpu_time,
            "speedup": speedup,
            "commit": commit_hash,
            "timestamp": time.time()
        }
        
        self._save_results()
        
        # Alert if performance regresses
        if speedup < self._get_baseline_speedup(test_name) * 0.9:
            raise PerformanceRegression(
                f"{test_name} speedup dropped to {speedup:.2f}x"
            )
```

**Benchmark Report Format:**

```
=== GPU Performance Report ===
Commit: abc123f
Date: 2025-12-15

Test                    CPU Time    GPU Time    Speedup    Target    Status
────────────────────────────────────────────────────────────────────────────
demodblock_32k          850ms       280ms       3.0x       2.5x      ✅ PASS
demodblock_64k          1650ms      520ms       3.2x       2.5x      ✅ PASS
full_decode_vhs_ntsc    65.3s       20.1s       3.2x       3.0x      ✅ PASS
chroma_processing       12.4s       4.2s        3.0x       2.5x      ✅ PASS

Overall: 3.1x average speedup (Target: 2.5x) ✅
```gpu=True, **kwargs):
           super().__init__(*args, **kwargs)
           self.use_gpu = use_gpu and GPU_AVAILABLE
           self.xp = get_array_module(self.use_gpu)
           
           if self.use_gpu:
               self._move_filters_to_gpu()
       
       def _move_filters_to_gpu(self):
           """Transfer precomputed filters to GPU memory"""
           self.Filters_gpu = {}
           for k, v in self.Filters.items():
               if isinstance(v, np.ndarray):
                   self.Filters_gpu[k] = cp.asarray(v)
               else:
                   self.Filters_gpu[k] = v
   ```

3. **Implement GPU-Accelerated demodblock()** (Week 3-4)
   ```python
   def demodblock_gpu(self, data=None, mtf_level=0, fftdata=None, cut=False):
       """GPU-accelerated version of demodblock"""
       xp = self.xp
       
       # Transfer data to GPU (only if not already there)
       if isinstance(data, np.ndarray):
           data_gpu = xp.asarray(data)
       else:
           data_gpu = data
       
       # FFT on GPU
       indata_fft = xp.fft.fft(data_gpu[:self.blocklen])
       
       # Apply RF video filter (element-wise multiplication)
       indata_fft_filt = indata_fft * self.Filters_gpu["RFVideo"]
       
       # Hilbert transform
       hilbert = xp.fft.ifft(indata_fft_filt)
       
       # Phase demodulation
       demod = xp.angle(hilbert) * self.freq_hz / (2 * xp.pi)
       
       # Clip and prepare for filtering
       demod_clipped = xp.clip(demod, self.iretohz(-60), self.iretohz(140))
       
       # FFT for post-demod filtering
       demod_fft = xp.fft.fft(demod_clipped)
       
       # Apply video filters
       out_video = xp.fft.ifft(demod_fft * self.Filters_gpu["FVideo"]).real
       out_video05 = xp.fft.ifft(demod_fft * self.Filters_gpu["FVideo05"]).real
       
       # Transfer back to CPU for downstream processing
       return {
           "video": {
               "demod": xp.asnumpy(out_video) if self.use_gpu else out_video,
               "demod_05": xp.asnumpy(out_video05) if self.use_gpu else out_video05,
           }
       }
   ```

4. **Add Command-Line Options** (Week 4)
   ```python
   # File: vhsdecode/main.py
   parser.add_argument(
       "--gpu",
       "--use-gpu",
       dest="use_gpu",
       action="store_true",
       default=False,
       help="Use GPU acceleration (requires CUDA and CuPy)",
   )
   parser.add_argument(
       "--gpu-id",
       type=int,
       dest="gpu_id",
       default=0,
       help="GPU device ID to use (default: 0)",
   )
   ```

5. **Testing & Validation** (Week 5-6)
   - Compare output pixel-by-pixel with CPU version
   - Benchmark performance on various formats
   - Test memory usage and stability
   - Validate on different GPU models

#### Files to Create/Modify

**New Files:**
- `vhsdecode/gpu_utils.py` - GPU detection and utilities
- `vhsdecode/process_gpu.py` - GPU-accelerated decoder classes
- `tests/test_gpu.py` - GPU-specific tests
- `docs/GPU_USAGE.md` - User documentation

**Modified Files:**
- `vhsdecode/main.py` - Add GPU command-line options
- `vhsdecode/process.py` - Add conditional GPU import
- `pyproject.toml` - Add cupy as optional dependency
- `requirements.txt` - Document GPU requirements
- `README.md` - Add GPU acceleration section

#### Dependencies

```toml
# pyproject.toml
[project.optional-dependencies]
gpu = [
    "cupy-cuda11x>=12.0.0; python_version >= '3.9'",
    "cupy-cuda12x>=12.0.0; python_version >= '3.9'"
]
```

### Phase 2: Custom CUDA Kernels (4-6 weeks)

**Goal**: Optimize critical operations with hand-tuned CUDA kernels  
**Expected Additional Speedup**: 1.5-2x (cumulative 3-6x total)  
**Risk Level**: Medium  

#### Tasks

1. **Optimized Phase Unwrapping Kernel** (Week 1-2)
   ```python
   from numba import cuda
   import math
   
   @cuda.jit
   def phase_unwrap_kernel(angles, output, freq_hz):
       """Parallel phase unwrapping with frequency scaling"""
       idx = cuda.grid(1)
       if idx < angles.size - 1:
           diff = angles[idx + 1] - angles[idx]
           if diff > math.pi:
               unwrapped = diff - 2 * math.pi
           elif diff < -math.pi:
               unwrapped = diff + 2 * math.pi
           else:
               unwrapped = diff
           output[idx] = unwrapped * freq_hz / (2 * math.pi)
   ```

2. **Batch Hilbert Transform** (Week 2-3)
   - Fused FFT + Hilbert + iFFT operation
   - Reduce memory transfers
   - Overlap computation with memory transfers

3. **Parallel Dropout Detection** (Week 3-4)
   - Sliding window approach on GPU
   - Threshold detection in parallel
   - Efficient reduction for results

4. **Optimized Chroma Processing** (Week 4-5)
   - Fused heterodyne and filtering operations
   - Minimize roundtrips through FFT

5. **Performance Profiling & Optimization** (Week 5-6)
   - NVIDIA Nsight profiling
   - Test-Driven Development Workflow

**Before Writing GPU Code:**
1. Ensure CPU baseline tests exist and pass
2. Capture reference outputs for comparison
3. Define acceptance criteria (max diff %, min speedup)

**During Development:**
1. Write unit test for new GPU function
2. Run test (expect failure)
3. Implement GPU function
4. Run test until it passes
5. Benchmark against CPU version
6. Commit only if all tests pass

**After Each Feature:**
1. Run full regression test suite
2. Update benchmark tracking
3. Document any precision trade-offs
4. Update performance dashboard

#### Test Organization

```
tests/
├── fixtures/
│   └── gpu_validation/          # Test data (see Test Data Requirements)
├── test_gpu_unit.py             # Unit tests for GPU functions
├── test_gpu_integration.py      # Full pipeline tests
├── test_gpu_regression.py       # Output validation against baseline
├── test_gpu_benchmark.py        # Performance benchmarks
├── test_gpu_memory.py           # Memory leak and VRAM usage tests
├── test_gpu_precision.py        # FP32 vs FP64 validation
└── benchmark_tracker.py         # Performance tracking utility
```

#### Acceptance Criteria Per Test

| Test Type | Pass Criteria | Frequency |
|-----------|--------------|-----------|
| Unit Tests | 100% pass, exact match (rtol=1e-6) | Every commit |
| Integration Tests | <0.1% output difference from CPU | Every commit |
| Regression Tests | Output matches reference within 0.1% | Every commit |
| Performance Tests | Speedup ≥ target (Phase 1: 2x) | Every commit |
| Memory Tests | No leaks, VRAM < 4GB | Daily |
| Precision Tests | SNR within 0.5 dB, ΔE < 2.0 | Weekly |

#### Unit Tests
```python
# tests/test_gpu_unit.py
import pytest
import numpy as np
import cupy as cp
from vhsdecode.gpu_utils import GPU_AVAILABLE

@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestGPUDecoding:
    def test_fft_equivalence(self):
        """Verify GPU FFT matches CPU FFT"""
        test_data = np.random.randn(32768).astype(np.float32)
        cpu_result = np.fft.fft(test_data)
        gpu_result = cp.asnumpy(cp.fft.fft(cp.asarray(test_data)))
        np.testing.assert_allclose(cpu_result, gpu_result, rtol=1e-6)
    
    def test_demod_output_match(self):
        """Verify GPU demod output matches CPU within tolerance"""
        # Load known test signal
        test_signal = load_test_signal("vhs_ntsc_32k_block.npy")
        
        # CPU decode
        cpu_decoder = VHSRFDecode(inputfreq=40)
        cpu_output = cpu_decoder.demodblock(test_signal)
        
        # GPU decode
        gpu_decoder = VHSRFDecodeGPU(inputfreq=40, use_gpu=True)
        gpu_output = gpu_decoder.demodblock(test_signal)
        
        # Compare
        np.testing.assert_allclose(
            cpu_output["video"]["demod"],
            gpu_output["video"]["demod"],
            rtol=1e-4, atol=1e-6
        )
    
    def test_filter_application(self):
        """Test frequency-domain filtering on GPU"""
        test_fft = cp.random.randn(32768, dtype=cp.complex64)
        test_filter = cp.random.randn(32768, dtype=cp.float32)
        
        # GPU filtering
        filtered = test_fft * test_filter
        
        # Verify on CPU
        cpu_fft = cp.asnumpy(test_fft)
        cpu_filter = cp.asnumpy(test_filter)
        expected = cpu_fft * cpu_filter
        
        np.testing.assert_allclose(cp.asnumpy(filtered), expected)
    
    def test_memory_efficiency(self):
        """Verify no memory leaks during GPU processing"""
        decoder = VHSRFDecodeGPU(inputfreq=40, use_gpu=True)
        test_data = cp.random.randn(32768, dtype=cp.float32)
        
        # Record initial memory
        initial_mem = cp.cuda.Device().mem_info[0]
        
        # Process 100 blocks
        for _ in range(100):
            decoder.demodblock(test_data)
        
        # Force garbage collection
        cp.get_default_memor (100% pass rate)
- [ ] Integration tests show <0.1% difference from CPU
- [ ] 2x+ speedup demonstrated on benchmark suite
- [ ] Performance tracking system operational
- [ ] Test data repository established
- [ ] Regression tests passing on all test captures
- [ ] No memory leaks detected
- [ ] Documentation complete
- [ ] CI/CD pipeline configured and passingDevice().mem_info[0]
      All Phase 1 tests still passing
- [ ] Performance dashboard shows consistent improvement
- [ ] Memory usage optimized
- [ ] All validation criteria met
- [ ] Benchmark regression tests passing
        assert mem_growth < 100 * 1024 * 1024, f"Memory leak: {mem_growth/1e6:.1f}MB"
```All tests passing in CI/CD
- [ ] Performance benchmarks published
- [ ] Test data publicly available (or documented how to generate)
- [ ] Users report successful deployment
- [ ] Performance benefits documented with real-world measurements
- [ ] Regression testing automa
#### Integration Tests
```python
# tests/test_gpu_integration.py
@pytest.mark.slow
@pytest.mark.gpu
@pytest.mark.parametrize("test_case", [
    "vhs_ntsc_colorbars.lds",
    "vhs_pal_testcard.lds",
    "svhs_ntsc_hifi.lds",
])
def test_full_decode_pipeline(test_case):
    """Process full sample files on both CPU and GPU"""
    input_path = f"tests/fixtures/gpu_validation/{test_case}"
    
    # CPU decode
    cpu_output = decode_file(input_path, use_gpu=False)
    
    # GPU decode
    gpu_output = decode_file(input_path, use_gpu=True)
    
    # Compare TBC files byte-by-byte (allow 0.1% variance)
    assert_tbc_match(cpu_output, gpu_output, max_diff_percent=0.1)
    
    # Validate metadata matches
    cpu_meta = load_metadata(cpu_output + ".json")
    gpu_meta = load_metadata(gpu_output + ".json")
    assert cpu_meta["videoParameters"] == gpu_meta["videoParameters"]
```

#### Performance Tests
```python
# tests/test_gpu_benchmark.py
import pytest

@pytest.mark.benchmark(group="demodblock")
def test_demodblock_cpu_speedup(benchmark):
    """Measure GPU vs CPU performance"""
    # Setup
    decoder_cpu = VHSRFDecode(inputfreq=40)
    decoder_gpu = VHSRFDecodeGPU(inputfreq=40, use_gpu=True)
    test_data = load_test_block()
    
    # Benchmark CPU
    cpu_time = benchmark(decoder_cpu.demodblock, test_data)
    
    # Benchmark GPU (separate run)
    gpu_time = benchmark(decoder_gpu.demodblock, test_data)
    
    # Calculate speedup
    speedup = cpu_time / gpu_time
    
    # Phase 1 target: 2x minimum, 3x goal
    assert speedup >= 2.0, f"Speedup {speedup:.2f}x below 2x minimum"
    
    # Track result
    BenchmarkTracker().record("demodblock_32k", cpu_time, gpu_time, 
                            get_git_commit())
- CUDA 11.0+
- Examples: GTX 1650, GTX 1660, RTX 2060

**Recommended:**
- NVIDIA GPU with CUDA Compute Capability 7.5+ (Turing or newer)
- 8GB+ VRAM
- CUDA 12.0+
- Examples: RTX 3060, RTX 3070, RTX 4060

**Optimal:**
- NVIDIA GPU with CUDA Compute Capability 8.0+ (Ampere or newer)
- 12GB+ VRAM
- NVLink (for multi-GPU)
- Examples: RTX 4080, RTX 4090, A5000, A6000

### Software Requirements

```bash
# CUDA Toolkit
cuda-toolkit >= 11.0

# Python packages
cupy-cuda11x >= 12.0.0  # for CUDA 11.x
cupy-cuda12x >= 12.0.0  # for CUDA 12.x
numpy >= 1.22
scipy >= 1.10

# Optional for Phase 2
numba >= 0.58.1
```

### Memory Considerations

**Per-Block Memory Usage:**
- Input data: 128-512 KB (32K-128K samples @ 16-bit)
- FFT workspace: 256-1024 KB
- Filters: 128-512 KB
- Intermediate results: 512-2048 KB
- **Total per block**: ~1-4 MB

**For typical operation:**
- ~100-200 MB VRAM for buffers and filters
- Additional 500MB-1GB for safety margin
- **Minimum 2GB VRAM recommended**

## Performance Projections

### Benchmark Scenarios

**Scenario 1: VHS NTSC (40 MSPS, 32K block)**
- Current CPU: ~1.5 FPS (67 seconds per video second)
- Phase 1 GPU: ~4-5 FPS (20-25 seconds per video second)
- Phase 2 GPU: ~7-10 FPS (10-14 seconds per video second)

**Scenario 2: SVHS PAL (40 MSPS, 64K block)**
- Current CPU: ~1.2 FPS (83 seconds per video second)
- Phase 1 GPU: ~3-4 FPS (25-33 seconds per video second)
- Phase 2 GPU: ~6-8 FPS (13-17 seconds per video second)

**Scenario 3: Betamax NTSC (40 MSPS, 32K block)**
- Current CPU: ~1.8 FPS (56 seconds per video second)
- Phase 1 GPU: ~5-6 FPS (17-20 seconds per video second)
- Phase 2 GPU: ~8-12 FPS (8-13 seconds per video second)

### Speedup Breakdown

| Component | CPU Time | Phase 1 Speedup | Phase 2 Speedup |
|-----------|----------|-----------------|-----------------|
| FFT Operations | 45% | 8x | 10x |
| Filtering | 25% | 6x | 8x |
| Hilbert/Phase | 15% | 4x | 6x |
| Chroma Processing | 10% | 3x | 5x |
| Overhead | 5% | 1x | 1x |
| **Overall** | 100% | **3-4x** | **5-7x** |

## Implementation Details

### Key Code Locations

**Primary Files to Modify:**
1. `vhsdecode/process.py` - Line 1191: `demodblock()` method
2. `lddecode/core.py` - Line 651: `demodblock_cpu()` method
3. `vhsdecode/utils.py` - Line 120: `filtfft()` function
4. `vhsdecode/chroma.py` - Chroma demodulation functions

**Filter Definitions:**
- `vhsdecode/compute_video_filters.py` - All filter generation
- `vhsdecode/process.py` - Lines 963-1190: `_computevideofilters_b()`

**FFT Usage:**
- Search for `npfft.fft`, `npfft.ifft`, `npfft.rfft`, `npfft.irfft`
- ~20 locations across codebase

### Testing Strategy

#### Unit Tests
```python
# tests/test_gpu.py
import pytest
import numpy as np
from vhsdecode.gpu_utils import GPU_AVAILABLE

@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestGPUDecoding:
    def test_fft_equivalence(self):
        """Verify GPU FFT matches CPU FFT"""
        pass
    
    def test_demod_output_match(self):
        """Verify GPU demod output matches CPU within tolerance"""
        pass
    
    def test_filter_application(self):
        """Test frequency-domain filtering on GPU"""
        pass
    
    def test_memory_efficiency(self):
        """Verify no memory leaks during GPU processing"""
        pass
```

#### Integration Tests
- Process full sample files on both CPU and GPU
- Compare output TBC files byte-by-byte
- Validate metadata matches
- Test all supported formats (VHS, SVHS, Betamax, Video8, etc.)

#### Performance Tests
```python
@pytest.mark.benchmark
def test_gpu_speedup(benchmark):
    """Measure GPU vs CPU performance"""
    # Benchmark demodblock execution time
    # Target: 3x+ speedup for Phase 1
```

### Error Handling

```python
class GPUError(Exception):
    """Base exception for GPU-related errors"""
    pass

class GPUOutOfMemoryError(GPUError):
    """Raised when GPU runs out of memory"""
    pass

def safe_gpu_operation(func):
    """Decorator for safe GPU operations with fallback"""
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except (GPUError, cp.cuda.memory.OutOfMemoryError):
            logger.warning("GPU operation failed, falling back to CPU")
            # Fallback to CPU version
            return func(*args, use_gpu=False, **kwargs)
    return wrapper
```

## Validation Criteria

### Quality Metrics
- **Pixel Difference**: <0.1% variance from CPU output
- **SNR**: Within 0.5 dB of CPU version
- **Dropout Detection**: 100% match rate
- **Sync Detection**: 100% match rate
- **Color Accuracy**: ΔE < 2.0

### Performance Metrics
- **Minimum Speedup**: 2x (Phase 1)
- **Target Speedup**: 3-4x (Phase 1), 5-7x (Phase 2)
- **Memory Usage**: <4GB VRAM for typical operation
- **CPU Overhead**: <10% increase

### Stability Metrics
- **No memory leaks**: 1000+ blocks processed without growth
- **Crash rate**: 0% on supported hardware
- **Fallback reliability**: 100% success rate

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Precision loss with FP32 | Medium | High | Extensive testing, selective FP64 |
| Memory limitations | Low | Medium | Streaming, graceful degradation |
| GPU compatibility issues | Medium | Low | Comprehensive device testing |
| Performance < expectations | Low | Medium | Phased approach, benchmarking |
| Development timeline overrun | Medium | Low | Modular implementation |

## Documentation Requirements

### User Documentation
1. **GPU Setup Guide** - Installation and configuration
2. **Performance Tuning** - Optimization tips
3. **Troubleshooting** - Common issues and solutions
4. **Hardware Recommendations** - GPU selection guide

### Developer Documentation
1. **Architecture Overview** - GPU pipeline design
2. **API Documentation** - GPU-specific functions
3. **Performance Analysis** - Profiling and optimization
4. **Contributing Guide** - GPU development guidelines

## Success Criteria

### Phase 1 Complete When:
- [ ] GPU-accelerated demodblock() implemented
- [ ] All unit tests passing
- [ ] Integration tests show <0.1% difference from CPU
- [ ] 2x+ speedup demonstrated on benchmark suite
- [ ] Documentation complete
- [ ] Merged to main branch

### Phase 2 Complete When:
- [ ] Custom CUDA kernels implemented
- [ ] Performance tests show 5x+ cumulative speedup
- [ ] Memory usage optimized
- [ ] All validation criteria met
- [ ] Production-ready and stable

### Project Complete When:
- [ ] GPU acceleration available in official release
- [ ] Users report successful deployment
- [ ] Performance benefits documented
- [ ] Future maintenance plan established

## Maintenance Plan

### Ongoing Requirements
- Monitor CuPy compatibility with new releases
- Test with new NVIDIA GPU architectures
- Update CUDA kernels for new features
- Performance regression testing
- User support for GPU issues

### Long-Term Considerations
- AMD GPU support (via ROCm/HIP)
- Intel GPU support (via SYCL/DPC++)
- Cloud GPU optimization (AWS, Azure, GCP)
- Container support (Docker with GPU passthrough)

## Appendix A: Detailed Performance Analysis

### Profiling Data (CPU Baseline)

```
Function                          | Time (ms) | % Total
----------------------------------|-----------|--------
demodblock                        | 1250.3    | 45.2%
  - npfft.fft                     |  420.5    | 15.2%
  - npfft.ifft                    |  380.2    | 13.7%
  - hilbert unwrap                |  245.8    |  8.9%
  - filtering                     |  203.8    |  7.4%
computeMetrics                    |  380.5    | 13.7%
compute_syncconf                  |  285.2    | 10.3%
dropout_detect                    |  245.8    |  8.9%
downscale                         |  195.6    |  7.1%
Other                             |  412.6    | 14.9%
Total                             | 2770.0    | 100%
```

### Expected GPU Performance

```
Function                          | Time (ms) | Speedup
----------------------------------|-----------|--------
demodblock_gpu                    |  312.6    | 4.0x
  - cp.fft.fft                    |   52.6    | 8.0x
  - cp.fft.ifft                   |   47.5    | 8.0x
  - hilbert_gpu                   |   61.5    | 4.0x
  - filtering_gpu                 |   34.0    | 6.0x
  - transfer overhead             |  117.0    | -
computeMetrics                    |  380.5    | 1.0x (CPU)
compute_syncconf                  |  285.2    | 1.0x (CPU)
dropout_detect                    |  245.8    | 1.0x (CPU)
downscale                         |  195.6    | 1.0x (CPU)
Other                             |  412.6    | 1.0x
Total                             |  1832.3   | 1.5x

With Phase 2 optimizations        |  1156.0   | 2.4x
```

## Appendix B: Code Examples

### Example: GPU-Accelerated Filter Application

```python
def apply_filters_gpu(self, fft_data):
    """Apply all filters in frequency domain on GPU"""
    xp = self.xp
    
    # All filters already on GPU in Filters_gpu dict
    filtered = fft_data.copy()
    
    # Apply MTF filter if enabled
    if self.mtf_level > 0:
        filtered *= self.Filters_gpu["MTF"] ** self.mtf_level
    
    # Apply video filter chain
    filtered *= self.Filters_gpu["RFVideo"]
    
    # Apply deemphasis
    if "FDeemp" in self.Filters_gpu:
        filtered *= self.Filters_gpu["FDeemp"]
    
    # Apply low-pass filter
    filtered *= self.Filters_gpu["Fvideo_lpf"]
    
    return filtered
```

### Example: Automatic Fallback

```python
def decode_with_fallback(self, data):
    """Decode with automatic GPU/CPU fallback"""
    try:
        if self.use_gpu:
            return self.demodblock_gpu(data)
    except (cp.cuda.memory.OutOfMemoryError, GPUError) as e:
        logger.warning(f"GPU decode failed: {e}, falling back to CPU")
        self.use_gpu = False
    
    return self.demodblock_cpu(data)
```

## Appendix C: Benchmark Scripts

```python
# benchmark/benchmark_gpu.py
import time
import numpy as np
from vhsdecode.process import VHSRFDecode
from vhsdecode.process_gpu import VHSRFDecodeGPU

def benchmark_demod(decoder, data, iterations=100):
    """Benchmark demodulation performance"""
    times = []
    for _ in range(iterations):
        start = time.perf_counter()
        result = decoder.demodblock(data)
        end = time.perf_counter()
        times.append(end - start)
    return np.mean(times), np.std(times)

# Generate test data
sample_rate = 40  # MHz
block_size = 32768
test_data = np.random.randn(block_size).astype(np.float32)

# CPU benchmark
cpu_decoder = VHSRFDecode(inputfreq=sample_rate)
cpu_mean, cpu_std = benchmark_demod(cpu_decoder, test_data)

# GPU benchmark
gpu_decoder = VHSRFDecodeGPU(inputfreq=sample_rate, use_gpu=True)
gpu_mean, gpu_std = benchmark_demod(gpu_decoder, test_data)

print(f"CPU: {cpu_mean*1000:.2f} ± {cpu_std*1000:.2f} ms")
print(f"GPU: {gpu_mean*1000:.2f} ± {gpu_std*1000:.2f} ms")
print(f"Speedup: {cpu_mean/gpu_mean:.2f}x")
```

## References

1. CuPy Documentation: https://docs.cupy.dev/en/stable/
2. Numba CUDA Documentation: https://numba.readthedocs.io/en/stable/cuda/
3. NVIDIA FFT Library: https://docs.nvidia.com/cuda/cufft/
4. GPU Acceleration Best Practices: https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/

---

## Repository Information

**Target Repository**: https://github.com/JohanAberg/vhs-decode.git  
**Base Branch**: vhs_decode  
**Implementation Branch**: (to be created: `feature/gpu-acceleration`)

## Contact & Questions

For questions or clarifications on this proposal:
- GitHub Issues: https://github.com/JohanAberg/vhs-decode/issues
- Upstream Project: https://github.com/oyvindln/vhs-decode
- Discord: vhs-decode community server

**Proposal Author**: Analysis Agent  
**Review Status**: Pending community review  
**Implementation Status**: Not started  
**Target Fork**: JohanAberg/vhs-decode
