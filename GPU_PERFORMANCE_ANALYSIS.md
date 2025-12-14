# GPU Performance Analysis - December 15, 2025

## Benchmark Results Summary

### Test Configuration
- **Samples Tested:** 2 VHS PAL captures
  - sony_short_40M_8b.u8 (2.77 GB)
  - sony_nichols_new_cable_v01.u8 (4.35 GB)
- **Test Length:** 100 frames each
- **Hardware:** NVIDIA RTX 4070 Ti (12GB VRAM)
- **Software:** CuPy 13.6.0, CUDA 12.6

### Performance Results

| Sample | Frames | CPU Time | GPU Time | Speedup | CPU FPS | GPU FPS |
|--------|--------|----------|----------|---------|---------|---------|
| sony_short_40M_8b | 100 | 2.63s | 3.01s | **0.87x** | 38.03 | 33.22 |
| sony_nichols_new_cable_v01 | 100 | 2.69s | 3.18s | **0.85x** | 37.22 | 31.47 |

**Average Speedup: 0.86x (GPU is 14% SLOWER than CPU)**

## Root Cause Analysis

### Issue: Excessive CPU↔GPU Data Transfers

The Phase 1 GPU implementation contains **17 transfer operations** per block:
- 5 transfers TO GPU (`transfer_to_gpu()`)
- 12 transfers FROM GPU (`transfer_from_gpu()`, `cp.asnumpy()`)

### Transfer Overhead Breakdown

Each transfer has ~0.1-1ms latency plus bandwidth cost:
- **PCIe 4.0 x16 bandwidth:** ~32 GB/s theoretical
- **Typical block size:** 32K-131K samples = 128KB-524KB
- **Transfer time per block:** 0.004-0.016ms (bandwidth) + 0.1-1ms (latency)
- **Total overhead per block:** ~2-17ms for 17 transfers

With blocks processed at high frequency, this overhead accumulates significantly.

### Why This Happens

Phase 1 implementation kept many operations on CPU to ensure correctness:
1. **Envelope filtering** - CPU (FEnvPost filter)
2. **FM demodulation** - CPU (unwrap_hilbert)
3. **High boost mixing** - Mixed CPU/GPU
4. **Spike replacement** - CPU
5. **Video filtering** - Partial GPU
6. **Debug plotting** - CPU (requires data copy)

## Optimization Opportunities for Phase 2

### High Impact Changes
1. **Keep entire pipeline on GPU** - Eliminate unnecessary transfers
   - Move envelope filtering to GPU
   - Move FM demodulation to GPU
   - Move spike replacement to GPU

2. **Batch operations** - Process multiple blocks on GPU before transferring
   - Current: Process 1 block → Transfer → Process next
   - Optimal: Process N blocks → Transfer all → Continue

3. **Use GPU memory pools** - Reduce allocation overhead
   - Pre-allocate working buffers
   - Reuse memory between blocks

4. **Async transfers** - Overlap computation and transfer
   - Use CUDA streams to pipeline operations
   - Transfer block N+1 while processing block N

### Expected Performance After Phase 2

Based on successful FFT benchmarks (6.5x-11.8x speedup):
- **Conservative estimate:** 3-4x overall speedup
- **Optimistic estimate:** 5-7x overall speedup
- **Target:** 4x minimum (current GPU tests show potential)

## Current Status: Phase 1 COMPLETE ✅

### What Works
✅ GPU detection and initialization
✅ Filter transfer to GPU
✅ FFT operations on GPU (validated 6.5x-11.8x speedup)
✅ Complex number operations on GPU
✅ Hilbert transform on GPU
✅ Error handling and CPU fallback
✅ Integration with decode pipeline
✅ Memory management

### Known Limitations
⚠️ Too many CPU↔GPU transfers (17 per block)
⚠️ Some operations still CPU-bound (FM demod, envelope filtering)
⚠️ No operation batching
⚠️ No async pipeline

## Recommendations

### Immediate Actions
1. **Profile transfer bottleneck** - Measure exact time spent in transfers
2. **Document Phase 2 plan** - Prioritize operations to move to GPU
3. **Implement GPU FM demodulation** - Biggest CPU bottleneck
4. **Add batching support** - Process multiple blocks per transfer

### Long-term Goals
1. **Full GPU pipeline** - <5 transfers per decode run
2. **CUDA kernels for custom ops** - Replace CuPy where needed
3. **Multi-GPU support** - Scale to multiple cards
4. **Benchmark suite** - Automated performance tracking

## Conclusion

The Phase 1 GPU implementation is **functionally correct** and successfully processes real VHS data. However, the excessive CPU↔GPU transfers create performance overhead that currently negates the GPU's computational advantages.

**Phase 2 optimization focus:** Reduce transfers from 17/block to <5/block by keeping the entire processing pipeline on GPU. This should achieve the target 3-6x speedup demonstrated in isolated FFT benchmarks.

**Status:** Phase 1 framework is production-ready for correctness validation. Phase 2 needed for performance gains.
