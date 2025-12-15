# Phase 2 Roadmap: Achieving GPU > CPU Performance

**Goal:** Make GPU decoder faster than single-threaded CPU (currently 25% slower)  
**Target:** 4-6 FPS (1.5-2x faster than single-threaded CPU at 2.98 FPS)  
**Current:** 2.25 FPS GPU vs 2.98 FPS CPU (1 thread)

## The 38% Problem: FM Demodulation

FM demodulation currently consumes **38% of GPU decode time** at 2.030ms per block, but should theoretically be ~0.5ms (7x faster than CPU). This single bottleneck is making the GPU slower than CPU overall.

### Root Cause Analysis

```python
# Current implementation in gpu_demod.py unwrap_hilbert_gpu()
def unwrap_hilbert_gpu(hilbert_gpu, freq_hz):
    tangles = cp.angle(hilbert_gpu)
    dangles[1:] = cp.diff(tangles)
    
    # PROBLEM #1: GPU→CPU sync!
    if dangles[0] < -cp.pi:  # This reads from GPU, stalls pipeline
        dangles[0] += tau
    
    # PROBLEM #2: Serial algorithm on parallel hardware
    tdangles2 = cp.unwrap(dangles)  # Phase unwrapping is inherently serial
```

**Why cp.unwrap() is slow:**
- Phase unwrapping requires sequential processing
- CuPy's implementation is not optimized for GPU
- Each element depends on previous element (anti-parallel)

## Phase 2 Tasks - Prioritized by Impact

### Task 1: Replace Phase Unwrapping Algorithm ⭐⭐⭐
**Impact:** 35% overall speedup (2.25 → 3.0+ FPS)  
**Effort:** Medium (3-4 hours)  
**Complexity:** Medium (CuPy-based, no CUDA C++)

**Approach:** Use **frequency discriminator** instead of phase unwrapping

**Current (phase-based):**
```python
# Slow: unwrap phase then differentiate
angles = cp.angle(hilbert)
unwrapped = cp.unwrap(angles)  # ← SERIAL, SLOW
freq = cp.diff(unwrapped) * freq_scale
```

**Proposed (frequency-based):**
```python
# Fast: compute instantaneous frequency directly
# f(t) = (1/2π) * d/dt[arg(z(t))]
# Using product rule: d/dt[arg(z)] = Im(z* dz/dt) / |z|²
z = hilbert_gpu
dz_dt = cp.diff(hilbert_gpu)
freq = cp.imag(cp.conj(z[:-1]) * dz_dt) / (cp.abs(z[:-1])**2)
freq *= freq_scale / (2 * cp.pi)
```

**Advantages:**
- Fully parallelizable (no sequential dependencies)
- No unwrapping needed
- Mathematically equivalent for well-behaved signals
- Single GPU kernel instead of multiple operations

**Expected Result:**
- FM demod: 2.030ms → 0.4-0.6ms (3-5x faster)
- Overall: 2.25 FPS → 3.2-3.5 FPS
- **GPU faster than single-threaded CPU!**

### Task 2: Eliminate GPU→CPU Synchronization ⭐⭐
**Impact:** 5-10% overall speedup  
**Effort:** Low (30 min)  
**Complexity:** Low

**Changes:**
```python
# BEFORE (causes sync):
if dangles[0] < -cp.pi:
    dangles[0] += tau

# AFTER (stays on GPU):
dangles = cp.where(
    (cp.arange(len(dangles)) == 0) & (dangles < -cp.pi),
    dangles + tau,
    dangles
)
```

### Task 3: Batch Block Processing ⭐
**Impact:** 10-15% overall speedup  
**Effort:** Medium (2-3 hours)  
**Complexity:** Medium

**Current:** Process one 32K block, return to Python, repeat
**Proposed:** Process 10 blocks in one GPU call

**Benefits:**
- Amortize Python call overhead (50-100μs per call)
- Better GPU utilization
- Opportunity for kernel fusion

**Implementation:**
```python
def demodblock_batch_gpu(self, data_list):
    """Process multiple blocks in one GPU call."""
    # Concatenate blocks
    data_batch = cp.concatenate([cp.asarray(d) for d in data_list])
    
    # Process all at once (existing code)
    results_batch = self._process_gpu_internal(data_batch)
    
    # Split results
    return [results_batch[i*blocklen:(i+1)*blocklen] 
            for i in range(len(data_list))]
```

### Task 4: Memory Transfer Optimization ⭐
**Impact:** 5-10% overall speedup  
**Effort:** Low (1 hour)  
**Complexity:** Low

**Current Issues:**
- 4.7x transfer asymmetry (2049MB GPU→CPU vs 170MB CPU→GPU)
- Suggests inefficient memory usage

**Solutions:**
1. Use pinned memory for faster transfers
2. Reuse GPU buffers instead of allocating per block
3. Transfer only final results, not intermediates

```python
# Allocate once, reuse
self.gpu_buffers = {
    'fft_data': cp.empty(self.blocklen, dtype=cp.complex128),
    'filtered': cp.empty(self.blocklen, dtype=cp.float64),
    'result': cp.empty(self.blocklen, dtype=cp.float32)
}
```

## Implementation Order

### Week 1: Core Performance
**Day 1-2:** Task 1 - Frequency discriminator (biggest impact)  
**Day 3:** Task 2 - Remove synchronization points  
**Day 4:** Profiling and validation  
**Expected:** 3.0-3.5 FPS (above CPU baseline!)

### Week 2: Optimization
**Day 1-2:** Task 3 - Batch processing  
**Day 3:** Task 4 - Memory optimization  
**Day 4:** Final profiling and benchmarks  
**Expected:** 4.0-5.0 FPS (1.3-1.7x faster than CPU)

### Week 3: Advanced (Optional)
- Custom CUDA kernels for fused operations
- Multi-GPU support
- Mixed precision (FP16/FP32/FP64)
**Expected:** 6-10 FPS (2-3x faster than CPU)

## Success Criteria

### Minimum (Must Achieve)
- [ ] GPU faster than single-threaded CPU (>2.98 FPS)
- [ ] FM demod < 25% of total time (currently 38%)
- [ ] Zero errors/fallbacks during decode

### Target (Should Achieve)
- [ ] GPU 1.5x faster than CPU (>4.5 FPS)
- [ ] FM demod < 15% of total time
- [ ] Memory transfers < 10% of total time

### Stretch (Nice to Have)
- [ ] GPU 2x faster than CPU (>6 FPS)
- [ ] Competitive with 8-thread CPU (3.11 FPS → beat with 1 GPU thread)
- [ ] Supports batch processing for multiple files

## Risk Assessment

### Low Risk
- Tasks 2 & 4: Minor changes, easy rollback
- Clear performance improvement expected

### Medium Risk
- Task 1: Algorithm change requires validation
- Need to verify output quality matches CPU
- Possible edge cases with noisy signals

### High Risk
- Task 3: Changes decoder architecture
- Might introduce threading bugs
- Requires extensive testing

## Validation Strategy

For each task:
1. **Correctness:** Decode reference sample, compare TBC byte-for-byte with CPU
2. **Performance:** Profile with --gpu-profile, verify expected speedup
3. **Quality:** Visual inspection of decoded video, check for artifacts
4. **Regression:** Run full test suite, ensure no existing features broken

**Test Samples:**
- sample_data/out2.u8 (PAL, clean signal)
- Real tape capture with dropouts
- NTSC sample
- SVHS high-bandwidth sample

## Code Locations

### To Modify
- `vhsdecode/gpu_demod.py` - FM demodulation (Task 1, 2)
- `vhsdecode/process_gpu.py` - Batch processing (Task 3)
- `vhsdecode/gpu_utils.py` - Memory management (Task 4)

### To Test
- `tests/test_gpu_unit.py` - Unit tests
- `tests/test_gpu_regression.py` - Output validation
- `tests/test_gpu_benchmark.py` - Performance tracking

## Expected Timeline

**Optimistic:** 4-5 days (Tasks 1-2 only)  
**Realistic:** 7-10 days (Tasks 1-4)  
**Conservative:** 14 days (Tasks 1-4 + validation + documentation)

## Decision Point

**Should we proceed with Phase 2?**

**Pros:**
- Clear path to beat CPU performance
- Task 1 alone should make GPU competitive
- Relatively low complexity (no CUDA C++ needed)
- Skills/infrastructure already in place

**Cons:**
- GPU still won't beat 8-thread CPU (need 3x improvement for that)
- Diminishing returns after Phase 2
- CPU decoder is "good enough" for most users

**Recommendation:** 
Proceed with **Task 1 only** (frequency discriminator) as a quick win. If it achieves expected speedup (3.0-3.5 FPS), continue with Tasks 2-4. If Task 1 disappoints, pause and reassess.

---

## Quick Start: Task 1 Implementation

### Frequency Discriminator Code

Replace `unwrap_hilbert_gpu()` in `vhsdecode/gpu_demod.py`:

```python
def unwrap_hilbert_gpu_v2(hilbert_gpu, freq_hz):
    """
    GPU-accelerated FM demodulation using frequency discriminator.
    
    Much faster than phase unwrapping because it's fully parallelizable.
    Uses instantaneous frequency formula: f(t) = (1/2π) Im(z* dz/dt) / |z|²
    """
    tau = 2.0 * cp.pi
    freq_scale = freq_hz / tau
    
    # Compute derivative (difference for discrete signal)
    dz_dt = cp.empty_like(hilbert_gpu)
    dz_dt[0] = 0.0
    dz_dt[1:] = cp.diff(hilbert_gpu)
    
    # Frequency discriminator: Im(conj(z) * dz/dt) / |z|²
    z_conj = cp.conj(hilbert_gpu)
    numerator = cp.imag(z_conj * dz_dt)
    denominator = cp.abs(hilbert_gpu) ** 2
    
    # Add small epsilon to avoid division by zero
    epsilon = 1e-10
    freq = numerator / (denominator + epsilon)
    
    # Scale to Hz
    freq *= freq_scale
    
    return freq
```

### Testing

```bash
# Test with new implementation
python decode.py vhs --gpu --length 11 --system PAL sample_data/out2.u8 test_freq_disc

# Compare output
diff test_freq_disc.tbc baseline_cpu_t1.tbc  # Should be nearly identical
```

### Expected Profile After Task 1

```
Operation                              Count    Total(ms)    Avg(ms)      %
----------------------------------------------------------------------
gpu_chroma_processing                   2732      3176.59      1.163  30.0%  ← Now largest
6_fm_demodulation                       2732      1500.00      0.549  15.0%  ← Reduced!
5_envelope_filter_gpu                   2732      2044.09      0.748  18.0%
1_data_transfer_to_gpu                  2732      1375.94      0.504  12.0%
...
----------------------------------------------------------------------
TOTAL                                            10500.00            100.0%
```

**Result:** 2.25 FPS → **3.2 FPS** (42% faster, beats single-threaded CPU!)

---

Ready to proceed with Phase 2? Start with Task 1 for maximum impact with minimum risk.
