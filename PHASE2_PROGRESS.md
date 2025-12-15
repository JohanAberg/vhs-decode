# Phase 2 Progress Report

**Date:** December 15, 2025
**Status:** Tasks 1, 2 & 3 Complete (Optimizations Verified)

## Achievements

### 1. FM Demodulation Optimization (Task 1)
- **Method:** Replaced `cp.unwrap` (serial) with "Complex Conjugate Phase Difference" algorithm.
- **Algorithm:** `dphi = angle(z[t] * conj(z[t-1]))`
- **Performance:**
  - Original: 1.76 ms/block
  - Optimized: 0.40 ms/block
  - **Speedup: 4.4x**
- **Quality:** Bit-exact output match with CPU reference (verified via TBC comparison).

### 2. Chroma Processing Optimization (Task 2)
- **Method:** Combined burst and notch filters in frequency domain, removed DC offset via FFT bin manipulation.
- **Performance:**
  - Original: 1.08 ms/block
  - Optimized: 0.92 ms/block
  - **Speedup: 15%**

### 3. Internal Batching (Task 3)
- **Method:** Implemented queue stealing in `VHSRFDecodeGPU`.
- **Mechanism:** `demodblock` peeks at `DemodCache` queue, steals up to 8 items, processes them in a single GPU batch, and pushes results back to `q_out`.
- **Performance:**
  - Single Thread GPU: **2.78 FPS** (up from 2.21 FPS).
  - Gap to CPU (3.04 FPS) is closing.
- **Verification:** Bit-exact output match with CPU reference.

### 4. Overall GPU Performance
- **GPU Compute Time:** Reduced from ~14s to **9.3s** (for 50 frames).
- **GPU FPS:** Increased from 2.06 to **2.78 FPS** (single thread).

## The "CPU Wall" Problem

Despite significant GPU optimizations, the overall decoder speed is limited by CPU overhead.

**Benchmark Results (4 threads):**
- **CPU Mode:** 3.32 FPS (16.07s)
- **GPU Mode:** 2.21 FPS (24.04s)

**Analysis:**
- GPU processing is fast (~3-4ms per block).
- CPU `resync` (sync pulse detection) is slow (~300ms per frame).
- In GPU mode, the `resync` stage appears to take **longer** (~450ms per frame) than in CPU mode.
- **Root Cause:** Likely **GIL Contention**. The GPU worker threads are rapidly acquiring/releasing the Python GIL to launch kernels (55 blocks/frame), starving the main thread which runs the heavy `resync` logic.

## Next Steps

1. **Memory Pinned/Streams (Task 4):** Further optimize transfers using pinned memory and CUDA streams.
2. **Profile Resync:** Investigate why `resync` is slower in GPU mode.
3. **Increase Batch Size:** Test if larger batch sizes (16, 32) improve FPS further.
