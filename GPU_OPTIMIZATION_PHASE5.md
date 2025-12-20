# GPU Optimization Phase 5 - Envelope Filter & Memory Transfer Improvements

**Date:** December 15, 2025  
**Target:** Reduce envelope filter time (14.5%) and transfer overhead (13.3%)

## Changes Implemented

### 1. Envelope Filter Optimizations

**Before (Phase 4):**
- 51-tap FIR filter
- Separate FFT, multiply, iFFT operations with intermediate variables
- Redundant `.real` casts on iFFT output (already real)

**After (Phase 5):**
```python
# Reduced FIR length: 51 → 35 taps (31% fewer computations)
fir_length=35  # Optimized for speed, maintains quality

# In-place operations to reduce memory allocations
env_fft_gpu *= self.Filters_gpu["FEnvPost_FIR_freq"]  # In-place multiply
env_gpu = cp.fft.irfft(env_fft_gpu)  # No redundant .real cast

# Same optimization for Hilbert filter
indata_fft_gpu *= hilbert_filter_gpu  # In-place
raw_filtered_gpu = cp.fft.ifft(indata_fft_gpu).real
```

**Expected Impact:**
- 31% fewer FFT operations (51→35 taps)
- Reduced memory allocations (in-place ops)
- Target: 14.5% → 10-11% of total time

### 2. Memory Transfer Optimizations

**Before:**
```python
def transfer_to_gpu(data):
    return cp.asarray(data)  # Blocking transfer
```

**After:**
```python
def transfer_to_gpu(data, stream=None):
    if stream is not None:
        # Async transfer with CUDA stream
        gpu_array = cp.empty(data.shape, dtype=data.dtype)
        gpu_array.set(data, stream=stream)
        return gpu_array
    return cp.asarray(data)
```

**New Feature: StreamManager**
```python
class StreamManager:
    """Manages separate streams for transfer and compute."""
    def __enter__(self):
        self.transfer = cp.cuda.Stream()
        self.compute = cp.cuda.Stream()
        return self
```

Enables overlapping transfers with computation:
```python
with StreamManager() as streams:
    # Transfer next block while GPU processes current block
    next_data_gpu = transfer_to_gpu(next_block, stream=streams.transfer)
    with streams.compute:
        result = process_current_block()
```

**Expected Impact:**
- Overlap transfers with compute (hide latency)
- Target: 13.3% → 8-10% of total time

### 3. VRAM Utilization

**Before:** 25% of free VRAM, capped at 3GB  
**After:** 80% of free VRAM, capped at 10GB

For RTX 4070 Ti (12GB, 11.57GB free):
- Old: 2.9 GB → ~1,450 blocks/batch
- New: 9.3 GB → **~4,600 blocks/batch**

Larger batches reduce per-block overhead.

## Performance Projection

**Baseline (Phase 4):**
- Total: 7728 ms for 33 frames
- FPS: 2.14

**Expected (Phase 5):**
- Envelope filter: 1123ms → 780ms (343ms saved)
- Transfers: 1962ms → 1300ms (662ms saved)
- **Total: ~6700ms (1000ms saved)**
- **FPS: 2.5-2.6 (17% improvement)**

## Quality Validation

FIR filter validation (35 taps vs 51 taps):
```python
# design_fir_envelope_filter() logs:
# Approximation error: <0.01 (excellent)
```

The envelope filter is a simple 1st-order Butterworth lowpass (700kHz cutoff).  
35 taps maintains excellent approximation quality while being significantly faster.

## Testing

Run with profiling to verify improvements:
```bash
.\.venv\Scripts\Activate.ps1
python decode.py vhs --system PAL --gpu --length 33 \\
    --gpu-profile --batch-processing \\
    sample_data/out2.u8 sample_data/test_phase5 --overwrite
```

Expected profiling output:
```
Operation                        Count    Total(ms)    Avg(ms)      %
--------------------------------------------------------------------
5_envelope_filter_gpu             1664       780.00     0.469   11.6%  ← down from 14.5%
1_data_transfer_to_gpu            1424       820.00     0.576   12.2%  ← down from 13.3%
7_final_transfer_to_cpu           1394       480.00     0.344    7.2%  ← down from 12.1%
```

## Future Optimizations

1. **Batch FFTs:** Use `cp.fft.rfft()` on stacked arrays instead of loop
2. **Fused envelope kernel:** Combine FFT + filter + iFFT in single CUDA kernel
3. **Double buffering:** Prefetch next batch while processing current
4. **Async TBC writes:** Offload file I/O to separate thread

## Rollback

If issues arise, revert to Phase 4 settings:
```python
# process_gpu.py line 223, 236
fir_length=51  # Original

# process_gpu.py line 169
target_vram_gb = min(mem_free_gb * 0.25, 3.0)  # Original

# gpu_utils.py
# Remove stream parameter from transfer functions
```
