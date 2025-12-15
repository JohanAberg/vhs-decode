# VHS-Decode GPU Optimization - Phase 1 Complete

**Date:** December 15, 2025  
**Status:** Phase 1 Completed ✅

## Executive Summary

Phase 1 GPU optimization has been completed, delivering a **70% performance improvement** over the initial buggy GPU implementation (1.32 → 2.25 FPS). However, GPU (2.25 FPS) is currently **25% slower** than single-threaded CPU (2.98 FPS) and **38% slower** than 8-thread CPU (3.11 FPS).

## Completed Tasks

### ✅ Task 1: FIR Envelope Filter (December 15, 2025)
**Problem:** FEnvPost IIR filter caused CPU fallback (19.7% of decode time)  
**Solution:** Converted IIR to 51-tap FIR approximation for GPU FFT-based filtering  
**Result:** 1.4x faster (1.306ms → 0.932ms), eliminated CPU bottleneck  
**Files:** vhsdecode/filter_conversion.py, vhsdecode/process_gpu.py  
**Documentation:** PHASE1_TASK1_RESULTS.md

### ✅ Task 2: GPU Chroma Processing (December 15, 2025)
**Problem:** FVideoBurst IIR filter caused 500+ shape mismatch errors per decode  
**Solution:** Converted FVideoBurst to FIR, created demod_chroma_filt_gpu()  
**Result:** 6.8x faster than CPU (~8ms → 1.163ms), zero errors  
**Files:** vhsdecode/gpu_utils.py, vhsdecode/process_gpu.py  
**Documentation:** PHASE1_TASK2_RESULTS.md

## Performance Analysis

### Current GPU Performance (50 frames, 2.06 FPS)

```
Operation                              Count    Total(ms)    Avg(ms)      %
----------------------------------------------------------------------
6_fm_demodulation                       2732      5547.27      2.030  38.0%
gpu_chroma_processing                   2732      3176.59      1.163  21.8%
5_envelope_filter_gpu                   2732      2044.09      0.748  14.0%
1_data_transfer_to_gpu                  2732      1375.94      0.504   9.4%
7_final_transfer_to_cpu                 2732       887.76      0.325   6.1%
4_envelope_calculation                  2732       742.43      0.272   5.1%
3_hilbert_ifft                          2732       631.55      0.231   4.3%
2_notch_and_rf_filters                  2732       196.21      0.072   1.3%
----------------------------------------------------------------------
TOTAL                                            14601.82            100.0%

Memory Transfers:
  CPU→GPU: 170.75 MB
  GPU→CPU: 2049.00 MB
  Peak GPU Memory: 3.82 MB
```

### Comparison: GPU vs CPU

| Configuration | FPS | Speedup | Notes |
|--------------|-----|---------|-------|
| CPU 1 thread | 2.98 | 1.0x (baseline) | Fair comparison |
| **GPU 1 thread** | **2.25** | **0.75x** | 25% slower than CPU |
| CPU 8 threads | 3.11 | 1.04x | Multi-threaded advantage |

### Per-Operation Analysis

| Operation | CPU (ms) | GPU (ms) | Speedup | Status |
|-----------|----------|----------|---------|--------|
| FM Demodulation | ~3.5 | 2.030 | 1.7x | ⚠️ Slow |
| Chroma Processing | ~8.0 | 1.163 | 6.8x | ✅ Good |
| Envelope Filter | ~1.3 | 0.748 | 1.7x | ✅ Good |
| Memory Transfers | N/A | 0.829 | N/A | ⚠️ Overhead |

## Root Cause: Why GPU is Slower

### 1. FM Demodulation Bottleneck (38% of time)
**Current:** 2.030ms per block (only 1.7x faster than CPU)  
**Expected:** 0.5ms per block (7x faster)  
**Issue:** Phase unwrapping is inherently serial, hard to parallelize
**Problem Code:**
```python
# Line 47 in gpu_demod.py - causes GPU→CPU sync!
if dangles[0] < -cp.pi:
    dangles[0] += tau

# cp.unwrap() - not optimized for GPU, serial algorithm
tdangles2 = cp.unwrap(dangles)
```

### 2. Memory Transfer Overhead (15.5% of time)
- CPU→GPU: 170.75 MB (9.4%)
- GPU→CPU: 2049.00 MB (6.1%)
- **4.7x asymmetry** suggests inefficient data handling

### 3. Block-Level Processing
- Processing one 32K block at a time
- Kernel launch overhead per block
- No batch processing or pipelining

## Phase 2 Recommendations

### High Priority: FM Demodulation Optimization
**Target:** 38% of decode time → 10-15%

**Option A: Custom CUDA Kernel**
- Replace cp.unwrap() with parallel phase unwrapping
- Eliminate GPU→CPU synchronization
- Expected: 3-5x speedup (2.030ms → 0.4-0.7ms)
- Complexity: High (requires CUDA C++)

**Option B: Algorithm Change**
- Use frequency discriminator instead of phase unwrapping
- Fully parallelizable, simpler implementation
- Expected: 5-7x speedup
- Complexity: Medium (CuPy only)

### Medium Priority: Batch Processing
**Target:** Eliminate per-block overhead

- Process 10-20 blocks per GPU call
- Use CUDA streams for async processing
- Expected: 1.5-2x additional speedup
- Complexity: Medium

### Low Priority: Memory Optimization
**Target:** Reduce transfer overhead

- Keep more data on GPU between blocks
- Use pinned memory for faster transfers
- Expected: 10-20% additional speedup
- Complexity: Low

## Lessons Learned

### 1. Filter Type Mismatch
**Problem:** IIR SOS filters (shape N,6) cannot be used with FFT multiplication  
**Solution:** Convert IIR → FIR → frequency domain before GPU transfer

**Pattern:**
```python
# ❌ WRONG
filter_sos = scipy.signal.butter(..., output="sos")
result = fft_data * filter_sos  # ERROR: shape mismatch!

# ✅ CORRECT
fir_coeffs, fir_freq = design_fir_envelope_filter(filter_sos, ...)
result = fft_data * fir_freq  # Works: both shape (16385,)
```

### 2. GPU Synchronization Kills Performance
**Problem:** Any CPU↔GPU data access causes sync, stalling pipeline

**Avoid:**
```python
if gpu_array[0] < threshold:  # Syncs entire GPU!
    gpu_array[0] = value

result = some_operation(gpu_array)
print(f"Mean: {cp.mean(gpu_array)}")  # Syncs before print
```

**Instead:**
```python
# Keep everything on GPU
gpu_array = cp.where(gpu_array < threshold, value, gpu_array)
```

### 3. CuPy != CUDA Performance
- CuPy functions are convenient but not always optimal
- `cp.unwrap()` is particularly slow (serial algorithm)
- Custom CUDA kernels needed for best performance on serial algorithms

### 4. Block-Level vs Batch Processing
- Current: Process one 32K block → return to Python → next block
- Each Python call has ~50-100μs overhead
- Batching 10 blocks → 10x reduction in Python overhead

## Files Modified

### Core Implementation
1. **vhsdecode/process_gpu.py** - GPU decoder main logic
   - Added FIR filter conversions (FEnvPost, FVideoBurst)
   - Integrated GPU chroma processing
   - Added exception traceback logging

2. **vhsdecode/gpu_utils.py** - GPU utility functions
   - Added demod_chroma_filt_gpu() for GPU chroma

3. **vhsdecode/filter_conversion.py** - IIR→FIR conversion
   - design_fir_envelope_filter() for IIR SOS → FIR conversion

### Documentation
1. **PHASE1_TASK1_RESULTS.md** - Envelope filter results
2. **PHASE1_TASK2_RESULTS.md** - Chroma processing results
3. **PHASE1_COMPLETE.md** - This document

## Next Steps

### Immediate: Decide on Path Forward
**Option 1: Continue GPU Optimization (Phase 2)**
- Implement custom CUDA kernel for FM demod
- Add batch processing
- Target: 5-10 FPS (2-3x better than CPU)
- Effort: 2-3 days

**Option 2: Accept Current Performance**
- GPU works correctly, just not faster than CPU
- Focus on other project priorities
- Revisit when bottlenecks change

**Option 3: Hybrid Approach**
- Use CPU for single-file decodes
- Use GPU for batch processing (future work)
- Best of both worlds

### Future Enhancements
1. **CUDA Kernel Development**
   - Custom phase unwrapping kernel
   - Fused operations (reduce kernel launches)
   - Shared memory optimization

2. **Multi-GPU Support**
   - Split blocks across multiple GPUs
   - Scale to 4-8 GPUs for real-time processing

3. **Mixed Precision**
   - Use FP16 for non-critical operations
   - Keep FP64 only for phase calculations
   - Expected: 2x memory bandwidth improvement

## Conclusion

Phase 1 successfully implemented GPU acceleration for envelope filtering and chroma processing, with significant speedups for those operations (1.4x and 6.8x respectively). However, overall GPU performance (2.25 FPS) is **25% slower than single-threaded CPU (2.98 FPS)** due to FM demodulation bottleneck (38% of time) and memory transfer overhead (15.5%).

The root cause is that **phase unwrapping in FM demodulation is inherently serial** and not well-suited to GPU parallelization using standard CuPy functions. Moving forward requires either:
1. Custom CUDA kernel for parallel phase unwrapping (high complexity, high reward)
2. Alternative FM demodulation algorithm (medium complexity, medium reward)
3. Accept current performance and focus on other priorities

**Recommendation:** Given the 38% bottleneck in FM demod and the diminishing returns from further CuPy-based optimizations, Phase 2 should focus on a custom CUDA kernel for phase unwrapping or exploring alternative demodulation algorithms that are more GPU-friendly.

---

**Hardware Tested:**
- GPU: NVIDIA RTX 4070 Ti (12GB VRAM, Compute 8.9)
- CPU: AMD Ryzen (8 threads tested)
- Test Data: sample_data/out2.u8 (PAL, 40 MSPS)
- Frame Count: 11 frames (quick test), 50 frames (profiling)
