# Phase 1, Task 2: GPU Chroma Processing - Results

## Completion Date
December 15, 2025

## Objective
Move chroma processing from CPU to GPU using FFT-based filtering to eliminate the 18.1% CPU bottleneck.

## Implementation

### Root Cause Analysis
The initial attempts failed with shape mismatch errors:
```
operands could not be broadcast together with shapes (16385,) (4, 6)
```

**Cause:** `FVideoBurst` filter was in IIR SOS format (scipy.butter output), not frequency-domain.

### Solution
1. **Filter Conversion:** Added FVideoBurst→FIR conversion in `_move_filters_to_gpu()` using existing `design_fir_envelope_filter()` utility
2. **GPU Function:** Created `demod_chroma_filt_gpu()` in gpu_utils.py for 100% GPU chroma processing
3. **Dual Path Update:** Modified both standard and optimized GPU demodblock functions

### Code Changes

#### vhsdecode/process_gpu.py
```python
# In _move_filters_to_gpu():
# Convert FVideoBurst from IIR to FIR for GPU chroma processing
logger.info("Converting FVideoBurst filter from IIR to FIR for GPU...")
burst_fir_coeffs, burst_fir_freq = design_fir_envelope_filter(
    self.Filters["FVideoBurst"],  # Original IIR in SOS format
    self.freq_hz,                 # Sample rate
    self.blocklen,                # FFT block size
    fir_length=51                 # FIR filter length
)
self.Filters["FVideoBurst_FIR"] = burst_fir_coeffs
self.Filters_gpu["FVideoBurst_freq"] = cp.asarray(burst_fir_freq)
```

```python
# In both _demodblock_gpu() and _demodblock_gpu_optimized():
out_chroma_gpu = demod_chroma_filt_gpu(
    chroma_source_gpu,
    self.Filters_gpu["FVideoBurst_freq"],  # Frequency-domain FIR
    self.blocklen,
    notch_gpu=notch_filter_gpu,  # FVideoNotchF or None
    do_notch=self._notch,
    move=int(self.options.chroma_offset),
)
```

#### vhsdecode/gpu_utils.py
```python
def demod_chroma_filt_gpu(data_gpu, filter_gpu, blocklen, notch_gpu=None, do_notch=False, move=10):
    """GPU-accelerated chroma demodulation using FFT-based filtering."""
    data_block = data_gpu[:blocklen]
    
    # Burst filter (FFT convolution)
    data_fft = cp.fft.rfft(data_block)
    out_chroma_fft = data_fft * filter_gpu
    out_chroma = cp.fft.irfft(out_chroma_fft).real
    
    # Optional notch filter
    if do_notch and notch_gpu is not None:
        out_chroma_fft = cp.fft.rfft(out_chroma)
        out_chroma_fft *= notch_gpu
        out_chroma = cp.fft.irfft(out_chroma_fft).real
    
    # Roll and DC removal
    out_chroma = cp.roll(out_chroma, move)
    out_chroma -= cp.mean(out_chroma)
    return out_chroma.astype(cp.float32)
```

## Performance Results

### Before Fix (GPU blocks failing → CPU fallback)
- Overall: **1.32 FPS**
- All GPU blocks erroring out with shape mismatch
- Falling back to CPU for every block

### After Fix (GPU chroma working)
- Overall: **2.25 FPS** (70% improvement)
- Zero GPU errors
- Chroma processing fully on GPU

### GPU Profile Breakdown (50 frames)
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
```

**Key Observations:**
- Chroma processing: 21.8% of time (1.163ms/block)
- This is 6.5x faster than CPU version (~7-8ms)
- Still slower than theoretical best due to data transfers

## Status

✅ **COMPLETE** - GPU chroma processing implemented and working

### Remaining Optimizations
- Chroma is functional but could be faster with better memory management
- **Next priority:** FM demodulation (38% of time) - needs CUDA kernel optimization

## Lessons Learned

### IIR vs Frequency-Domain Filters
**Critical Pattern:** IIR SOS filters cannot be directly used in FFT multiplication

```python
# ❌ WRONG - IIR SOS format (shape: 4,6)
filter_sos = scipy.signal.butter(..., output="sos")
result_fft = data_fft * filter_sos  # ERROR: shape mismatch!

# ✅ CORRECT - Convert to FIR, then frequency domain
fir_coeffs, fir_freq = design_fir_envelope_filter(filter_sos, ...)
result_fft = data_fft * fir_freq  # Works: both shape (16385,)
```

### Filter Types in VHS-Decode
- **FVideoBurst:** IIR (scipy.butter) - needs conversion
- **FVideoNotch:** IIR (scipy.butter) - needs FVideoNotchF variant
- **FVideo, FVideo05:** Already frequency-domain (filtfft)
- **RFVideo, Hilbert:** Already frequency-domain (filtfft)

### Debugging GPU Errors
Add traceback logging to exception handlers:
```python
except Exception as e:
    import traceback
    logger.error(f"Error: {e}")
    logger.error(f"Traceback: {traceback.format_exc()}")
```

## Files Modified
1. `vhsdecode/process_gpu.py` - Filter conversion + GPU chroma calls
2. `vhsdecode/gpu_utils.py` - New demod_chroma_filt_gpu() function
3. `vhsdecode/filter_conversion.py` - Reused for FVideoBurst

## Next Steps
**Phase 1, Task 3:** Optimize FM demodulation (38% of time, 2.030ms/block)
- Current: CPU unwrap_hilbert()
- Target: Custom CUDA kernel for phase unwrapping
- Expected: 3-5x speedup (2.030ms → 0.4-0.7ms)
- This will push GPU ahead of CPU performance
