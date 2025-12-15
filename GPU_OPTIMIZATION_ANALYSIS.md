# GPU Optimization Analysis

## Profiling Results (50 frames, PAL)

### Single Thread (Optimal)
**Total Time:** 15.24 seconds  
**Decode Speed:** 2.13 FPS  
**Blocks Processed:** ~2705  
**Peak GPU Memory:** 3.32 MB  

### Four Threads (Comparison)
**Total Time:** 11.47 seconds  
**Decode Speed:** 1.79 FPS ⚠️ **Slower per-frame**  
**Blocks Processed:** Variable per thread  
**Peak GPU Memory:** 14.76 MB  

**Finding:** GPU decoding works best with 1-2 threads. More threads add synchronization overhead and reduce throughput.  

### Performance Breakdown

| Operation | Time (ms) | % | Avg/Block (ms) |
|-----------|-----------|---|----------------|
| FM Demodulation | 7049 | 46.2% | 2.607 |
| Envelope Filter (CPU) | 3387 | 22.2% | 1.252 |
| Data Transfer to GPU | 1939 | 12.7% | 0.717 |
| Final Transfer to CPU | 933 | 6.1% | 0.345 |
| Envelope Calculation | 931 | 6.1% | 0.344 |
| Hilbert IFFT | 772 | 5.1% | 0.286 |
| Notch/RF Filters | 231 | 1.5% | 0.085 |

### Memory Transfer Analysis

- **CPU → GPU:** 507 MB (187 KB/block)
- **GPU → CPU:** 2366 MB (875 KB/block)
- **Transfer Ratio:** 4.7x more data coming back than sent
- **Peak GPU Memory:** 3.32 MB (very efficient!)

## Optimization Opportunities

### 1. FM Demodulation (46.2% of time) 🎯 **PRIORITY**

**Current:** `unwrap_hilbert_gpu()` takes 2.6ms per block

**Analysis:**
- This is the single largest bottleneck
- Likely involves phase unwrapping and complex math operations
- May have sub-optimal memory access patterns or kernel launches

**Optimization Strategies:**
```python
# A. Fuse operations to reduce kernel launches
- Combine Hilbert transform + phase unwrapping into single kernel
- Use CuPy fused kernels or custom CUDA kernels

# B. Check if using efficient algorithms
- Review phase unwrapping algorithm (is it optimized for GPU?)
- Consider alternative FM demod algorithms (e.g., atan2 approximations)

# C. Memory access optimization
- Ensure coalesced memory access in GPU kernels
- Use shared memory for frequently accessed data
```

**Expected Speedup:** 2-3x (reduce from 2.6ms to ~0.9-1.3ms)

### 2. Envelope Filter on CPU (22.2% of time) 🎯

**Current:** IIR filter (`FEnvPost`) runs on CPU, requires GPU→CPU→GPU transfers

**Why It's On CPU:**
- IIR/SOS filters require time-domain sequential processing
- Standard GPU FFT-based filtering doesn't work for IIR filters

**Optimization Strategies:**
```python
# A. GPU IIR Implementation (HIGH IMPACT)
- Implement parallel IIR filter using CuPy/CUDA
- Use methods like:
  * Parallel scan algorithms for forward/backward passes
  * Transposed direct form II for better parallelism
  * Split into cascaded biquads processed in parallel

# B. Replace with FIR Approximation
- Design FIR filter that approximates IIR response
- Can use FFT-based convolution on GPU (already fast)
- Trade-off: longer filter length for similar frequency response

# C. Keep Data on GPU Between Operations
- If multiple IIR operations, batch them to reduce transfers
```

**Expected Speedup:** 5-8x with GPU IIR (reduce from 1.25ms to ~0.15-0.25ms)

### 3. Memory Transfer Asymmetry (18.8% combined)

**Problem:** Transferring 4.7x more data back than sent

**Analysis:**
- Input: Raw RF samples (typically uint8 or float32)
- Output: Multiple arrays (video, video05, chroma, envelope)
- Output arrays are often larger and float64

**Optimization Strategies:**
```python
# A. Reduce precision where possible
- Use float32 instead of float64 for intermediate results
- Only convert to float64 at final output if needed

# B. Selective transfer
- Only transfer data that's actually needed on CPU
- Keep intermediate results on GPU between blocks

# C. Compression/Decimation
- Decimate or downsample data before transfer if appropriate
- Transfer only ROI (region of interest) data

# D. Async transfers
- Use CUDA streams to overlap transfer with computation
```

**Expected Speedup:** 1.5-2x (reduce transfer overhead)

### 4. Envelope Calculation (6.1% of time)

**Current:** `cp.abs()` and `cp.roll()` operations

**Optimization:**
```python
# Fuse abs() and roll() into single kernel
envelope_gpu = cp.ElementwiseKernel(
    'complex128 raw',
    'float32 env',
    'env = abs(raw)',
    'abs_kernel'
)
```

**Expected Speedup:** 1.5-2x minor improvement

## Prioritized Action Plan

### Phase 1: Quick Wins (1-2 days)
1. **Profile unwrap_hilbert_gpu in detail** - Use CuPy profiler to identify sub-operations
2. **Fuse small operations** - Combine abs/roll, reduce kernel launches
3. **Check data types** - Ensure using float32 where float64 not needed

### Phase 2: GPU IIR Filter (3-5 days)
1. **Implement parallel IIR filter** using parallel scan or biquad cascade
2. **Benchmark against CPU** - Verify speedup
3. **Fallback logic** - Keep CPU version as backup

### Phase 3: FM Demod Optimization (1 week)
1. **Analyze unwrap_hilbert_gpu algorithm** - Identify bottlenecks
2. **Custom CUDA kernel** - Fuse operations, optimize memory access
3. **Alternative algorithms** - Test different FM demod methods
4. **Benchmark** - Aim for 2-3x speedup

### Phase 4: Advanced Optimizations (ongoing)
1. **Async transfers** - Overlap compute and data movement
2. **Multi-stream processing** - Process multiple blocks concurrently
3. **Kernel fusion** - Combine more operations
4. **Memory pool tuning** - Optimize CuPy memory allocation

## Expected Performance Gains

| Optimization | Current (ms) | Target (ms) | Speedup |
|--------------|--------------|-------------|---------|
| FM Demod | 2.607 | 0.9-1.3 | 2-3x |
| Envelope Filter | 1.252 | 0.15-0.25 | 5-8x |
| Transfers | 1.062 | 0.5-0.7 | 1.5-2x |
| **Total per block** | **5.64** | **~2.0** | **~2.8x** |

**Overall Expected:** 2.13 FPS → **5.9 FPS** (2.8x faster)

With full optimization: **6-8 FPS** (3-4x faster than current)

## Profiling Commands

```bash
# Enable profiling with current implementation
python decode.py vhs --system PAL --gpu --gpu-profile --length 50 input.u8 output

# Detailed CuPy profiling
CUDA_LAUNCH_BLOCKING=1 python -m cProfile -o profile.stats decode.py vhs ...

# NVIDIA profiler
nvprof python decode.py vhs --gpu --length 50 input.u8 output
nsys profile python decode.py vhs --gpu --length 50 input.u8 output
```

## Next Steps

1. **Profile unwrap_hilbert_gpu** in detail to understand its internals
2. **Implement GPU IIR filter** as proof-of-concept
3. **Benchmark each optimization** individually to measure gains
4. **Document GPU kernel improvements** for community review

## Hardware Reference

- **GPU:** NVIDIA RTX 4070 Ti (or similar from test)
- **Peak Memory:** 3.32 MB (very low utilization - can handle larger blocks/batches)
- **Compute Capability:** 8.9+ (Ampere/Ada architecture)

