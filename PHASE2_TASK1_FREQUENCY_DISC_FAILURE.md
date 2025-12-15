# Phase 2 Task 1: Frequency Discriminator Implementation - Failure Analysis

**Date:** December 15, 2025  
**Status:** ❌ FAILED - Reverted  
**Reason:** Output completely broken - unable to find field sync

## What Was Attempted

Replaced `unwrap_hilbert_gpu()` phase unwrapping algorithm with frequency discriminator approach using formula:

```python
f(t) = (1/2π) Im(z* dz/dt) / |z|²
```

This was expected to be 3-5x faster because it's fully parallelizable (no sequential cp.unwrap()).

## Implementation

```python
def unwrap_hilbert_gpu(hilbert_gpu, freq_hz):
    tau = 2.0 * cp.pi
    freq_scale = freq_hz / tau
    
    # Compute derivative
    dz_dt = cp.empty_like(hilbert_gpu)
    dz_dt[0] = 0.0
    dz_dt[1:] = cp.diff(hilbert_gpu)
    
    # Frequency discriminator
    z_conj = cp.conj(hilbert_gpu)
    numerator = cp.imag(z_conj * dz_dt)
    denominator = cp.abs(hilbert_gpu) ** 2
    epsilon = 1e-10
    freq = numerator / (denominator + epsilon)
    
    # Scale to Hz
    freq *= freq_scale
    return freq
```

## Test Results

**Command:**
```bash
python decode.py vhs --system PAL -f 40 --threads 1 --gpu --length 11 --overwrite sample_data/out2.u8 test_freq_disc
```

**Output:** 500+ "Unable to determine start of field - dropping field" errors

**Failure Mode:** Decoder unable to find vertical sync → cannot assemble fields → zero frames decoded

**Comparison:**
- Original (cp.unwrap): 2.04 FPS, 11 frames decoded successfully ✅
- Frequency discriminator: 0 FPS, 0 frames decoded ❌

## Root Cause Analysis

### Mathematical Issues

1. **Missing DC Offset Handling**
   - Original code uses `cp.unwrap()` which tracks cumulative phase
   - Frequency discriminator computes instantaneous frequency
   - **Problem:** No integration or offset correction to match expected signal range

2. **Scaling Mismatch**
   - VHS-Decode expects specific frequency range (e.g., 3.8-4.8 MHz for PAL luma)
   - Frequency discriminator output may have wrong scale or offset
   - **Problem:** Formula `freq_scale = freq_hz / tau` might not match decoder expectations

3. **Phase Wrapping Artifacts**
   - Original code explicitly handles phase jumps with `cp.unwrap()` and modulo operations
   - Frequency discriminator skips unwrapping entirely
   - **Problem:** May introduce discontinuities at ±π boundaries

### Signal Processing Issues

**VHS RF Signal Characteristics:**
- Luma carrier: ~4.3 MHz (NTSC), ~3.8-4.8 MHz (PAL)
- Chroma subcarrier: ~627 kHz downconverted
- **Critical requirement:** Precise frequency tracking for TBC (timebase correction)

**What the Original Algorithm Does:**
```python
tangles = cp.angle(hilbert_gpu)          # [-π, π]
dangles = cp.diff(tangles)                # Angle differences
tdangles2 = cp.unwrap(dangles)            # Remove 2π jumps
tdangles2 = cp.fmod(tdangles2, tau)       # Modulo [0, 2π]
tdangles2 *= freq_scale                   # Scale to Hz
```

**What Frequency Discriminator Does:**
```python
freq = Im(z* dz/dt) / |z|²
freq *= freq_scale
```

**Missing Steps:**
- No unwrapping (handles 2π ambiguity)
- No DC offset correction
- No integration (freq discriminator gives instantaneous freq, not phase)

## Why Frequency Discriminator Failed

### Theory vs Practice

**Theoretical Equivalence:**
```
Phase: φ(t) = ∫ ω(t) dt
Instantaneous frequency: ω(t) = dφ/dt

Therefore: Im(z* dz/dt) / |z|² should equal dφ/dt
```

**But in discrete signals:**
- cp.unwrap() provides phase with discontinuities removed
- Frequency discriminator provides raw instantaneous frequency
- **Missing:** Integration step to go from frequency back to phase
- **Missing:** DC offset calibration for VHS signal range

### VHS-Decode Requirements

The decoder expects demodulated output to be in **specific frequency range**:
- PAL luma: 3.8-4.8 MHz center ~4.3 MHz
- Signal processing chain expects this range for:
  - Sync pulse detection (hsync/vsync)
  - Color burst detection
  - Timebase correction

**What went wrong:** Frequency discriminator output was in wrong range → sync detection failed → no frames decoded

## Lessons Learned

### Algorithm Substitution Requirements

**Before replacing cp.unwrap(), must ensure:**

1. ✅ **Mathematical equivalence** - frequency discriminator is theoretically correct
2. ❌ **Scaling calibration** - output must match decoder expectations
3. ❌ **DC offset handling** - signal must be in correct absolute range
4. ❌ **Edge case handling** - what does original do at boundaries?
5. ❌ **Integration requirements** - does decoder need phase or frequency?

**We only validated #1, not #2-5.**

### Testing Methodology

**What we should have done:**

1. **Unit test**: Compare 1000 samples of original vs new output
   ```python
   original = unwrap_hilbert_gpu_original(hilbert_gpu, freq_hz)
   new = unwrap_hilbert_gpu_discriminator(hilbert_gpu, freq_hz)
   diff = cp.abs(original - new)
   max_error = cp.max(diff)  # Should be < 1 Hz
   ```

2. **Range validation**: Check output is in expected frequency range
   ```python
   expected_range = (3.8e6, 4.8e6)  # PAL luma
   assert cp.min(freq) > expected_range[0]
   assert cp.max(freq) < expected_range[1]
   ```

3. **Signal inspection**: Plot first 1000 samples and compare visually

4. **Incremental testing**: Test with single field before full decode

**Instead:** Jumped straight to full 11-frame decode → discovered complete failure

## Path Forward

### Option 1: Fix Frequency Discriminator ⚠️

**Required work:**
- Add integration step: `phase = cp.cumsum(freq * dt)`
- Calibrate DC offset to match original output
- Add unit tests comparing outputs
- **Estimated time:** 4-6 hours
- **Risk:** Medium (might still have subtle errors)

### Option 2: Optimize cp.unwrap() 🔍

**Investigate:**
- Is cp.unwrap() actually slow? (Profile it separately)
- Can we replace with custom CUDA kernel?
- Are the modulo/where operations the real bottleneck?
- **Estimated time:** 2-3 hours investigation
- **Risk:** Low (incremental optimization)

### Option 3: Different Bottleneck 💡

**Hypothesis:** FM demod might not be the real problem.

**Evidence from profiling:**
- FM demod: 38% of time (2.030ms)
- But GPU overall: 2.25 FPS vs CPU 2.98 FPS (only 25% slower)
- **Question:** Is 38% because FM demod is slow, or because everything else got faster?

**Relative speedups:**
- Envelope filter: 1.7x faster than CPU
- Chroma: 6.8x faster than CPU
- FM demod: 1.7x faster than CPU (est. ~3.5ms CPU → 2.0ms GPU)

**Insight:** FM demod speedup (1.7x) matches envelope filter speedup (1.7x).  
Maybe both are hitting the same GPU limitation (memory bandwidth? kernel launch overhead?).

**Action:** Profile memory transfers and kernel launch overhead before continuing.

### Option 4: Accept Current Performance ✅

**Pragmatic assessment:**
- GPU decoder works correctly (zero errors)
- GPU faster than 1-thread CPU: 2.25 FPS vs 2.04 FPS (10% gain)
- GPU slower than 8-thread CPU: 2.25 FPS vs 3.11 FPS (38% loss)
- **Question:** Is this good enough?

**Use cases:**
- Real-time decoding: No (need 25 FPS for PAL, 30 FPS for NTSC)
- Batch processing: Maybe (depends on tape length)
- GPU value proposition: ❌ Not compelling vs multi-thread CPU

## Recommendations

**Immediate:** ✋ STOP trying to optimize FM demod

**Instead:**

1. **Profile memory transfers separately** (might be 50% of time)
2. **Investigate kernel launch overhead** (are we launching too many small kernels?)
3. **Consider batch processing** (decode multiple blocks at once)
4. **Measure end-to-end vs per-operation** (where is the real bottleneck?)

**Only after understanding true bottleneck:** Continue with targeted optimization.

## Status Update

**Phase 2 Task 1: ❌ FAILED**
- Frequency discriminator produces wrong output
- Decoder cannot find sync
- Reverted to original cp.unwrap() implementation

**Next steps:** Investigate WHY GPU is slower before continuing optimization attempts.

**Time spent:** 1 hour (implementation + testing + analysis)

**Decision point:** Should we continue Phase 2 optimizations, or accept current GPU performance?

