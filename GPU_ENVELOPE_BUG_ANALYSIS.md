# GPU Envelope Filtering Bug Analysis

## Problem Summary

The GPU implementation has a fundamental design error in the `envelope_filter_gpu()` function in [vhsdecode/gpu_demod.py](vhsdecode/gpu_demod.py#L125-L160).

## Root Cause

**Filter Type Mismatch:**
- The `FEnvPost` filter is a **Second-Order Sections (SOS) IIR filter** (Butterworth lowpass)
- Format: `[[b0, b1, b2, a0, a1, a2]]` shape (1, 6)
- Must be applied in **time domain** using difference equations

**Incorrect GPU Implementation:**
```python
# Current WRONG code in gpu_demod.py
env_fft = cp.fft.rfft(raw_env_gpu)  # Real FFT
filtered_fft = env_fft * filter_coeffs  # Trying to multiply SOS as freq filter
return cp.fft.irfft(filtered_fft, n=len(raw_env_gpu))
```

**Why It Fails:**
1. SOS filter (1, 6) doesn't match FFT length (N//2+1)
2. SOS filters are not frequency-domain coefficients
3. Length mismatch causes: `"Out shape is mismatched"` error
4. Even with padding/truncation, the math is fundamentally wrong

## Current Behavior

Every envelope filtering operation fails and falls back to CPU:
```
GPU envelope filtering failed: Out shape is mismatched, using CPU fallback
```

This happens **hundreds of times** during decode, causing:
- Constant CPU↔GPU transfers
- Negated GPU performance benefits
- Actually **slower** than pure CPU (overhead exceeds gains)

## Correct CPU Implementation

[vhsdecode/utils.py](vhsdecode/utils.py#L20):
```python
def filter_simple(data, filter_coeffs):
    return signal.sosfiltfilt(filter_coeffs, data, padlen=70)
```

Uses `scipy.signal.sosfiltfilt()` which correctly applies SOS filters bidirectionally.

## Solutions

### Option 1: Use CuPy Signal Processing (RECOMMENDED)
CuPy doesn't have `sosfiltfilt` built-in, but we can implement IIR filtering on GPU:
- Use `cupyx.scipy.signal.sosfilt()` if available
- Or implement biquad cascade manually on GPU
- Proper zero-phase filtering requires forward-backward pass

### Option 2: Keep CPU Fallback (CURRENT - ACCEPTABLE)
The current fallback to CPU actually works correctly. The "bug" is just the warning spam and unnecessary GPU→CPU→GPU transfers.

**Quick Fix:** Remove the broken GPU implementation entirely and always use CPU for this specific filter.

### Option 3: Pre-compute Frequency Domain Equivalent
Convert the IIR filter to a long FIR approximation:
- Use `scipy.signal.sosfreqz()` to get frequency response
- Create frequency-domain filter for FFT multiplication
- Requires careful handling of filter length and phase

## Recommended Immediate Fix

**Remove the broken GPU path** for envelope filtering:

```python
# In vhsdecode/process_gpu.py around line 467-478
# REMOVE the try/except, always use CPU for this filter

env_filter = self.Filters["FEnvPost"]
env = np.array(transfer_from_gpu(raw_env_gpu))
from vhsdecode import utils
env = utils.filter_simple(env, env_filter).astype(np.single)
env_mean = np.mean(env)
env_gpu = transfer_to_gpu(env)
del raw_env_gpu
```

This will:
- ✅ Eliminate hundreds of error messages
- ✅ Still produce correct output
- ✅ Potentially improve performance by avoiding failed GPU attempts
- ✅ Keep code simple until proper GPU IIR filtering is implemented

## Long-term Fix (Phase 2)

Implement proper GPU-accelerated IIR filtering:
1. Port `sosfiltfilt` logic to CuPy
2. Implement biquad cascade on GPU
3. Handle forward-backward pass for zero-phase
4. Benchmark to ensure actual speedup

## Additional Bug: `to_begin` TypeError

The error `"Unexpected error in GPU demodblock: 'to_begin' should be of type cupy.ndarray"` is **already fixed** in [vhsdecode/gpu_demod.py](vhsdecode/gpu_demod.py#L44):

```python
dangles = cp.ediff1d(tangles, to_begin=cp.array([0]))  # ✅ Correct
```

This was documented in [PHASE2_TEST_RESULTS.md](PHASE2_TEST_RESULTS.md#L197) and fixed.

## Testing the Fix

After applying the immediate fix:
```bash
python decode.py vhs --system PAL -f 40 --threads 1 --length 11 --gpu sample_data/out2.u8 out2
```

Expected result:
- No "GPU envelope filtering failed" warnings
- Clean decode output
- GPU used for FFT/Hilbert/other operations
- CPU used only for envelope filter (documented limitation)
