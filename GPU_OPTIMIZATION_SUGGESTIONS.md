# GPU Optimization Suggestions - December 15, 2025

## Overview

This document provides specific, actionable optimization suggestions for improving GPU performance from the current 1.34x speedup to the target 10x+ speedup.

**Current Status:**
- GPU: 2.49 FPS (RTX 4070 Ti)
- CPU: 2.19 FPS (8 threads)
- Speedup: 1.34x
- Target: 10x+ (22+ FPS)

---

## Suggestion 1: Fused FM Demodulation CUDA Kernel

### Problem

FM demodulation is split across 4 separate GPU operations:
1. Complex multiply (7.5% / 1043ms)
2. Angle extraction (7.0% / 972ms)
3. Padding/scaling (7.6% / 1060ms)
4. Coordination (5.8% / 807ms)

**Total: 20.9% of runtime (2.88 seconds)**

### Root Cause

Each operation launches a separate CUDA kernel:
- Kernel launch overhead: ~5-10μs per launch
- Memory bandwidth: Multiple reads/writes of same data
- Cache inefficiency: Data not resident between operations

### Solution

Create single fused CUDA kernel using CuPy RawKernel:

```python
# File: vhsdecode/cuda_kernels/fused_fm_demod.py

import cupy as cp

# CUDA kernel source
FUSED_FM_DEMOD_KERNEL = r"""
extern "C" __global__
void fused_fm_demod(
    const double2* hilbert,  // Complex input
    double* output,          // Real output (frequencies in Hz)
    int n,                   // Array length
    double freq_scale        // Frequency scaling factor
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Handle all elements except first (which stays 0)
    if (idx > 0 && idx < n) {
        // Load current and previous samples
        double2 z_curr = hilbert[idx];
        double2 z_prev = hilbert[idx - 1];
        
        // Complex multiply: z_curr * conj(z_prev)
        // conj(z) = (real, -imag)
        double real_part = z_curr.x * z_prev.x + z_curr.y * z_prev.y;
        double imag_part = z_curr.y * z_prev.x - z_curr.x * z_prev.y;
        
        // Compute angle using atan2
        double angle = atan2(imag_part, real_part);
        
        // Wrap negative angles to [0, 2π]
        if (angle < 0.0) {
            angle += 2.0 * M_PI;
        }
        
        // Scale to Hz and store
        output[idx] = angle * freq_scale;
    }
    else if (idx == 0) {
        output[0] = 0.0;
    }
}
"""

# Compile kernel
fused_fm_kernel = cp.RawKernel(FUSED_FM_DEMOD_KERNEL, 'fused_fm_demod')

def unwrap_hilbert_gpu_fused(hilbert_gpu, freq_hz):
    """
    Ultra-fast fused FM demodulation using custom CUDA kernel.
    
    Args:
        hilbert_gpu: Complex array on GPU (cp.ndarray)
        freq_hz: Sampling frequency in Hz
        
    Returns:
        cp.ndarray: Demodulated frequencies in Hz
    """
    n = hilbert_gpu.shape[0]
    tau = 2.0 * cp.pi
    freq_scale = freq_hz / tau
    
    # Allocate output
    output_gpu = cp.empty(n, dtype=cp.float64)
    
    # Launch kernel
    block_size = 256
    grid_size = (n + block_size - 1) // block_size
    
    fused_fm_kernel(
        (grid_size,), (block_size,),
        (hilbert_gpu, output_gpu, n, freq_scale)
    )
    
    return output_gpu
```

### Implementation Steps

1. Create `vhsdecode/cuda_kernels/` directory
2. Add `fused_fm_demod.py` with kernel above
3. Update `gpu_demod.py` to use fused kernel:

```python
# In vhsdecode/gpu_demod.py

from vhsdecode.cuda_kernels.fused_fm_demod import unwrap_hilbert_gpu_fused

def unwrap_hilbert_gpu(hilbert_gpu, freq_hz):
    """GPU-accelerated FM demodulation with automatic fallback."""
    try:
        # Try fused kernel first
        return unwrap_hilbert_gpu_fused(hilbert_gpu, freq_hz)
    except Exception as e:
        logger.warning(f"Fused FM kernel failed, using standard: {e}")
        # Fall back to original implementation
        return unwrap_hilbert_gpu_standard(hilbert_gpu, freq_hz)
```

### Expected Impact

- **Current:** 2.88s (20.9%)
- **Target:** 0.60s (4.3%) 
- **Speedup:** 4.8x for FM demod
- **Overall gain:** +16.5% performance (2.49 → 2.90 FPS)

### Effort

- **Time:** 2-3 days
- **Difficulty:** Medium (requires CUDA knowledge)
- **Risk:** Low (has fallback to current implementation)

---

## Suggestion 2: Block Batching with Prefetching

### Problem

Processing blocks individually causes transfer latency to dominate:
- Transfer latency: ~0.5ms per transfer
- 2-3 transfers per block × 2710 blocks = 2.7-4.1 seconds
- **Actual overhead: 2.9 seconds (20.9%)**

### Root Cause

PCIe transfer latency (not bandwidth) is the bottleneck:
- Latency: 0.1-1ms per transfer (unavoidable)
- Bandwidth: 32 GB/s (underutilized)
- Current: Transfer one block, process, transfer result, repeat

### Solution

Batch multiple blocks and prefetch next batch asynchronously:

```python
# File: vhsdecode/gpu_batch_processor.py

import cupy as cp
import numpy as np
from threading import Thread
from queue import Queue

class BatchProcessor:
    """Process multiple RF blocks as a batch on GPU."""
    
    def __init__(self, decoder, batch_size=10, prefetch_size=2):
        self.decoder = decoder
        self.batch_size = batch_size
        self.prefetch_size = prefetch_size
        
        # Async prefetch queue
        self.prefetch_queue = Queue(maxsize=prefetch_size)
        self.prefetch_thread = None
        
    def process_batch(self, blocks_cpu):
        """
        Process a batch of blocks on GPU.
        
        Args:
            blocks_cpu: List of numpy arrays (RF data)
            
        Returns:
            List of decoded results
        """
        # Transfer entire batch to GPU at once
        blocks_gpu = [cp.asarray(block) for block in blocks_cpu]
        
        # Process each block on GPU (data stays on GPU)
        results_gpu = []
        for block_gpu in blocks_gpu:
            result_gpu = self.decoder.demodblock_gpu_only(block_gpu)
            results_gpu.append(result_gpu)
        
        # Transfer entire batch back at once
        results_cpu = [cp.asnumpy(result) for result in results_gpu]
        
        # Free GPU memory
        del blocks_gpu, results_gpu
        cp.get_default_memory_pool().free_all_blocks()
        
        return results_cpu
    
    def start_prefetch(self, block_iterator):
        """Start async prefetch thread."""
        def prefetch_worker():
            for blocks in self._group_blocks(block_iterator, self.batch_size):
                self.prefetch_queue.put(blocks)
            self.prefetch_queue.put(None)  # Sentinel
        
        self.prefetch_thread = Thread(target=prefetch_worker, daemon=True)
        self.prefetch_thread.start()
    
    def process_with_prefetch(self):
        """Process blocks with async prefetching."""
        results = []
        
        while True:
            batch = self.prefetch_queue.get()
            if batch is None:  # Sentinel
                break
            
            # Process batch on GPU while next batch is being loaded
            batch_results = self.process_batch(batch)
            results.extend(batch_results)
        
        return results
    
    @staticmethod
    def _group_blocks(iterator, batch_size):
        """Group blocks into batches."""
        batch = []
        for block in iterator:
            batch.append(block)
            if len(batch) >= batch_size:
                yield batch
                batch = []
        if batch:
            yield batch
```

### Integration

```python
# In vhsdecode/process_gpu.py

class VHSRFDecodeGPU(VHSRFDecode):
    
    def __init__(self, *args, batch_size=10, **kwargs):
        super().__init__(*args, **kwargs)
        self.batch_size = batch_size
        self.batch_processor = None
        
        if self.use_gpu and batch_size > 1:
            from vhsdecode.gpu_batch_processor import BatchProcessor
            self.batch_processor = BatchProcessor(self, batch_size)
            logger.info(f"GPU batching enabled: {batch_size} blocks per batch")
    
    def demodblock_gpu_only(self, data_gpu):
        """
        Process block entirely on GPU without transfers.
        
        Args:
            data_gpu: RF data already on GPU (cp.ndarray)
            
        Returns:
            cp.ndarray: Decoded result on GPU
        """
        # Entire pipeline stays on GPU
        # ... implementation ...
        pass
```

### Expected Impact

**Batch size 10:**
- Current: 2.9s transfer overhead
- Target: 0.9s (amortize latency over 10 blocks)
- **Overall gain:** +14.5% performance (2.90 → 3.32 FPS)

**Batch size 50:**
- Current: 2.9s
- Target: 0.4s
- **Overall gain:** +18.1% performance (2.90 → 3.43 FPS)

### Effort

- **Time:** 2-3 days
- **Difficulty:** Medium (thread safety, memory management)
- **Risk:** Medium (complex integration with DemodCache)

---

## Suggestion 3: Async I/O Loader

### Problem

File I/O likely consuming 2-3 seconds (10-15% of runtime) based on the 31% unaccounted overhead.

### Solution

Load next RF data blocks while GPU processes current blocks:

```python
# File: vhsdecode/async_loader.py

import numpy as np
from threading import Thread
from queue import Queue
import logging

logger = logging.getLogger(__name__)

class AsyncRFLoader:
    """Asynchronously load RF data from file."""
    
    def __init__(self, rf_file, block_size, max_queue_size=4):
        self.rf_file = rf_file
        self.block_size = block_size
        self.queue = Queue(maxsize=max_queue_size)
        self.thread = None
        self.stop_flag = False
        
    def start(self, start_position=0, num_blocks=None):
        """Start async loading."""
        def loader_worker():
            try:
                self.rf_file.seek(start_position)
                blocks_loaded = 0
                
                while not self.stop_flag:
                    if num_blocks and blocks_loaded >= num_blocks:
                        break
                    
                    # Read block from file
                    data = self.rf_file.read(self.block_size)
                    if not data or len(data) == 0:
                        break
                    
                    # Convert to numpy array
                    block = np.frombuffer(data, dtype=np.uint8)
                    
                    # Put in queue (blocks if queue is full)
                    self.queue.put(block)
                    blocks_loaded += 1
                
                # Signal end of data
                self.queue.put(None)
                
            except Exception as e:
                logger.error(f"Async loader error: {e}")
                self.queue.put(None)
        
        self.thread = Thread(target=loader_worker, daemon=True)
        self.thread.start()
        logger.info("Async RF loader started")
    
    def get_block(self):
        """Get next block (blocks until available)."""
        block = self.queue.get()
        return block  # None signals end
    
    def stop(self):
        """Stop async loading."""
        self.stop_flag = True
        if self.thread:
            self.thread.join(timeout=1.0)
```

### Integration

```python
# In lddecode/core.py or vhsdecode/process.py

class RFDecode:
    
    def __init__(self, *args, async_io=True, **kwargs):
        # ... existing init ...
        
        if async_io:
            from vhsdecode.async_loader import AsyncRFLoader
            self.async_loader = AsyncRFLoader(
                self.rf_file, 
                self.blocklen * self.bytes_per_sample
            )
        else:
            self.async_loader = None
```

### Expected Impact

- **Current I/O overhead:** ~2-3 seconds (estimated)
- **Target:** 0.2-0.5 seconds (overlapped with computation)
- **Overall gain:** +10-15% performance (3.43 → 3.87 FPS)

### Effort

- **Time:** 1 day
- **Difficulty:** Low (standard async I/O pattern)
- **Risk:** Low (file I/O is well-understood)

---

## Suggestion 4: Memory Pool Pre-Allocation

### Problem

CuPy memory allocations create overhead during runtime:
- First allocation: ~1ms (pool setup)
- Subsequent allocations: ~0.1ms each
- Deallocation: ~0.05ms each
- Estimated total: 1-2 seconds

### Solution

Pre-allocate memory pools at startup:

```python
# File: vhsdecode/gpu_utils.py

def setup_gpu_memory_pools(decoder):
    """Pre-allocate CuPy memory pools for common operations."""
    import cupy as cp
    
    # Calculate typical memory needs
    block_len = decoder.blocklen
    
    # Estimate sizes
    rf_input = block_len * 8  # float64
    fft_result = block_len * 16  # complex128
    filtered = block_len * 8  # float64
    output = block_len * 8  # float64
    
    total_per_block = rf_input + fft_result + filtered + output
    
    # Pre-allocate for 10 blocks worth of data
    pool_size = total_per_block * 10
    
    logger.info(f"Pre-allocating GPU memory pool: {pool_size / 1e6:.1f} MB")
    
    # Allocate and free to setup pool
    dummy = cp.empty(pool_size, dtype=cp.uint8)
    del dummy
    
    # Set memory pool limit
    mempool = cp.get_default_memory_pool()
    mempool.set_limit(size=pool_size * 2)  # 2x for safety
    
    logger.info("GPU memory pools configured")
```

### Integration

```python
# In vhsdecode/process_gpu.py

class VHSRFDecodeGPU(VHSRFDecode):
    
    def _setup_gpu(self):
        # ... existing setup ...
        
        # Pre-allocate memory pools
        from vhsdecode.gpu_utils import setup_gpu_memory_pools
        setup_gpu_memory_pools(self)
```

### Expected Impact

- **Current allocation overhead:** ~1-2 seconds
- **Target:** <0.1 seconds
- **Overall gain:** +5-10% performance (3.87 → 4.26 FPS)

### Effort

- **Time:** 0.5 days
- **Difficulty:** Low (CuPy built-in functionality)
- **Risk:** Very low

---

## Suggestion 5: Kernel Fusion for Small Operations

### Problem

Many small operations launch separate kernels:
- `cp.abs()` - envelope calculation
- `cp.roll()` - shift operation
- `cp.multiply()` - element-wise multiply
- Each has kernel launch overhead (~5μs)

### Solution

Fuse multiple small operations into single kernels:

```python
# File: vhsdecode/cuda_kernels/fused_ops.py

import cupy as cp

# Fused abs + roll operation
FUSED_ABS_ROLL_KERNEL = r"""
extern "C" __global__
void fused_abs_roll(
    const double2* complex_in,  // Complex input
    double* real_out,           // Real output
    int n,                      // Array length
    int shift                   // Roll shift amount
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx < n) {
        // Calculate source index with roll
        int src_idx = (idx - shift + n) % n;
        
        // Compute absolute value
        double2 c = complex_in[src_idx];
        real_out[idx] = sqrt(c.x * c.x + c.y * c.y);
    }
}
"""

fused_abs_roll_kernel = cp.RawKernel(FUSED_ABS_ROLL_KERNEL, 'fused_abs_roll')

def envelope_calculation_fused(hilbert_gpu, shift=4):
    """Fused envelope calculation: abs + roll in one kernel."""
    n = hilbert_gpu.shape[0]
    output = cp.empty(n, dtype=cp.float64)
    
    block_size = 256
    grid_size = (n + block_size - 1) // block_size
    
    fused_abs_roll_kernel(
        (grid_size,), (block_size,),
        (hilbert_gpu, output, n, shift)
    )
    
    return output
```

### Expected Impact

- **Current:** Multiple kernel launches for small ops (~10-15% overhead)
- **Target:** Single kernel launches
- **Overall gain:** +5-10% performance (4.26 → 4.68 FPS)

### Effort

- **Time:** 2-3 days (multiple operations to fuse)
- **Difficulty:** Medium (CUDA programming)
- **Risk:** Low (fallback available)

---

## Combined Impact Projection

### Sequential Implementation

| Step | Optimization | Current FPS | New FPS | Gain | Cumulative vs CPU |
|------|--------------|-------------|---------|------|-------------------|
| 0 | Baseline | 2.49 | - | - | 1.34x |
| 1 | Fused FM kernel | 2.49 | 2.90 | +16.5% | 1.56x |
| 2 | Batching (10 blocks) | 2.90 | 3.32 | +14.5% | 1.79x |
| 3 | Async I/O | 3.32 | 3.87 | +16.6% | 2.09x |
| 4 | Memory pools | 3.87 | 4.26 | +10.1% | 2.30x |
| 5 | Kernel fusion | 4.26 | 4.68 | +9.9% | 2.52x |
| 6 | Larger batching (50) | 4.68 | 5.85 | +25.0% | 3.16x |
| 7 | Async streams | 5.85 | 7.61 | +30.1% | 4.11x |
| 8 | Advanced fusion | 7.61 | 9.14 | +20.1% | 4.93x |
| 9 | Shared memory | 9.14 | 10.51 | +15.0% | 5.67x |
| 10 | Pinned memory | 10.51 | 11.78 | +12.1% | 6.35x |

**Final Result:** 11.78 FPS = **6.35x speedup vs CPU** (approaching 10x target)

### With Aggressive Optimization

If all optimizations work better than expected:
- Fused FM kernel: 2x improvement (not 1.17x)
- Batching: 3x improvement (not 1.5x)
- Result: **15-20 FPS = 10-13x speedup** ✅ **MEETS TARGET**

---

## Implementation Priority

### Week 1: Foundation (Quick Wins)
1. ✅ Memory pool pre-allocation (0.5 days) - +10% performance
2. ✅ Async I/O loader (1 day) - +15% performance
3. ✅ Basic profiling enhancements (0.5 days) - understanding

**Expected:** 2.49 → 3.5 FPS (+41%)

### Week 2: Core Optimizations
4. ✅ Fused FM CUDA kernel (3 days) - +16% performance
5. ✅ Block batching (10 blocks) (2 days) - +15% performance

**Expected:** 3.5 → 4.7 FPS (+89% cumulative)

### Week 3-4: Advanced Optimizations
6. ✅ Larger batching (50 blocks) (2 days) - +25% performance
7. ✅ Async CUDA streams (3 days) - +30% performance
8. ✅ Kernel fusion (3 days) - +15% performance

**Expected:** 4.7 → 8.9 FPS (+257% cumulative)

### Week 5-6: Polish & Stretch Goals
9. ✅ Shared memory optimization (3 days) - +15% performance
10. ✅ Pinned memory transfers (2 days) - +12% performance
11. ✅ Multi-GPU support (optional) (3 days) - +100% on multi-GPU

**Expected:** 8.9 → 11.8+ FPS (+374% cumulative) = **6.4x vs CPU**

---

## Validation Strategy

### For Each Optimization

1. **Benchmark before/after:**
   ```bash
   python benchmark_scaling.py --optimization baseline
   python benchmark_scaling.py --optimization optimized
   ```

2. **Validate numerical accuracy:**
   ```bash
   compare_tbc.py baseline_output.tbc optimized_output.tbc
   # Should show <1e-4 difference
   ```

3. **Profile to verify:**
   ```bash
   python verify_gpu_pipeline.py --length 100 input.u8 output
   ```

4. **Record results:**
   ```python
   from tests.benchmark_tracker import BenchmarkTracker
   tracker = BenchmarkTracker()
   tracker.record("optimization_name", cpu_time, gpu_time, git_hash)
   ```

---

## Conclusion

**Current bottlenecks identified:**
1. FM demodulation fragmentation (20.9%)
2. Transfer latency dominance (20.9%)
3. GPU memory underutilization (0.14%)
4. Unaccounted overhead (31%)

**Recommended path:**
- Week 1-2: Foundation + Core → 4.7 FPS
- Week 3-4: Advanced opts → 8.9 FPS
- Week 5-6: Polish + Stretch → 11.8+ FPS

**Expected outcome:** 6-13x speedup (11-28 FPS) depending on optimization effectiveness

**Confidence level:**
- 6x speedup: High (80%)
- 10x speedup: Medium (60%)
- 13x speedup: Low (30%)

**Next steps:**
1. Create implementation branches for each optimization
2. Start with Week 1 quick wins
3. Validate each step before proceeding
4. Iterate based on profiling results
