# GPU Profiling Guide for VHS-Decode

## Quick Start

### Basic Profiling

```bash
# Activate virtual environment
.\.venv\Scripts\Activate.ps1

# Run with GPU profiling enabled
python decode.py vhs --system PAL --gpu --gpu-profile --length 50 sample_data/out2.u8 output
```

### Output Example

```
======================================================================
GPU PROFILING SUMMARY
======================================================================

Operation                              Count    Total(ms)    Avg(ms)      %
----------------------------------------------------------------------
6_fm_demodulation                       2704      7049.07      2.607  46.2%
5_envelope_filter_cpu                   2705      3387.36      1.252  22.2%
1_data_transfer_to_gpu                  2705      1939.49      0.717  12.7%
...
----------------------------------------------------------------------

Memory Transfers:
  CPU→GPU: 507.19 MB
  GPU→CPU: 2366.25 MB
  Peak GPU Memory: 3.32 MB
======================================================================
```

## Profiling Operations Measured

1. **1_data_transfer_to_gpu** - Initial RF data upload to GPU
2. **2_notch_and_rf_filters** - RF and notch filter application (FFT domain)
3. **3_hilbert_ifft** - Inverse FFT for Hilbert transform
4. **4_envelope_calculation** - Envelope detection (abs + roll)
5. **5_envelope_filter_cpu** - IIR envelope filtering (CPU fallback)
6. **6_fm_demodulation** - FM demodulation and phase unwrapping
7. **7_final_transfer_to_cpu** - Transfer results back to CPU

## Advanced Profiling

### Detailed CuPy Profiling

```bash
# Enable CUDA synchronous mode for accurate timing
$env:CUDA_LAUNCH_BLOCKING = "1"
python decode.py vhs --gpu --gpu-profile --length 50 input.u8 output
```

### Python Profiler Integration

```python
import cProfile
import pstats

cProfile.run(
    'decode_with_gpu()',
    'profile.stats'
)

# Analyze results
stats = pstats.Stats('profile.stats')
stats.sort_stats('cumulative')
stats.print_stats(20)
```

### NVIDIA Nsight Systems

```bash
# Comprehensive GPU profiling with Nsight
nsys profile --stats=true python decode.py vhs --gpu --length 50 input.u8 output

# View in GUI
nsight-sys profile.qdrep
```

### NVIDIA Nsight Compute

```bash
# Detailed kernel-level profiling
ncu --set full python decode.py vhs --gpu --length 10 input.u8 output
```

## Interpreting Results

### Time Distribution

- **High %** operations are optimization targets
- **> 30%** = Critical bottleneck
- **10-30%** = Significant optimization opportunity
- **< 10%** = Minor improvements possible

### Memory Transfer Patterns

- **CPU→GPU > GPU→CPU** = Good (minimizing output transfers)
- **GPU→CPU >> CPU→GPU** = Bad (current issue: 4.7x ratio)
- **Peak GPU Memory** = Check for memory leaks or excessive allocation

### Per-Block Averages

- Compare against theoretical limits
- Identify outlier operations
- Guide kernel optimization priorities

## Profiling Best Practices

### 1. Consistent Test Conditions

```bash
# Always use same parameters for comparison
--system PAL
--length 50
--threads 1
--gpu
```

### 2. Warm-up Runs

```bash
# First run initializes GPU - discard results
# Second run for accurate measurements
```

### 3. Multiple Runs

```bash
# Run 3-5 times, take average
for i in {1..5}; do
    python decode.py vhs --gpu --gpu-profile --length 50 input.u8 output_$i
done
```

### 4. Compare Before/After

```bash
# Baseline
git checkout main
python decode.py vhs --gpu --gpu-profile --length 50 input.u8 baseline

# Optimized
git checkout optimization-branch
python decode.py vhs --gpu --gpu-profile --length 50 input.u8 optimized

# Compare results
```

## Optimization Workflow

1. **Profile** - Identify bottleneck
2. **Hypothesize** - Why is it slow?
3. **Implement** - Make targeted change
4. **Measure** - Re-profile to verify
5. **Iterate** - Repeat for next bottleneck

## Common Issues

### Profiling Overhead

Profiling adds ~5-10% overhead due to synchronization

```python
# Disable profiling for production
python decode.py vhs --gpu --length 1000 input.u8 output  # No --gpu-profile
```

### Memory Pool Warnings

```
WARNING: GPU memory pool not freed
```

Solution: Add explicit cleanup calls or use context managers

### Inaccurate Timings

If CUDA_LAUNCH_BLOCKING=0, GPU operations overlap - use =1 for profiling

## Thread Count Impact (GPU)

| Threads | FPS | Per-Block Time | Peak GPU Memory | Notes |
|---------|-----|----------------|-----------------|-------|
| 1 | 2.13 | 5.6 ms | 3.3 MB | **Optimal for GPU** |
| 4 | 1.79 | Variable | 14.8 MB | Synchronization overhead |

**Recommendation:** Use 1-2 threads for GPU decoding. More threads add overhead without performance gain.

## Performance Targets

| Component | Current (1T) | Current (4T) | Target | World-Class |
|-----------|--------------|--------------|--------|-------------|
| FM Demod | 2.6 ms (46%) | 1.8 ms (28%) | 0.9 ms | 0.5 ms |
| Env Filter | 1.3 ms (22%) | 0.9 ms (16%) | 0.2 ms | 0.1 ms |
| Transfers | 1.1 ms (19%) | 1.2 ms (28%) | 0.6 ms | 0.3 ms |
| **Total** | **5.6 ms** | **Variable** | **2.0 ms** | **1.0 ms** |
| **FPS** | **2.1** | **1.8** | **5.9** | **11.8** |

## Tools Summary

| Tool | Use Case | Output |
|------|----------|--------|
| `--gpu-profile` | Quick overview | Text summary |
| `cProfile` | Python-level profiling | Call graph |
| `nsys` | GPU timeline | Visual timeline |
| `ncu` | Kernel optimization | Detailed metrics |
| CuPy profiler | CuPy-specific | Timing breakdown |

## Related Documentation

- [GPU_OPTIMIZATION_ANALYSIS.md](GPU_OPTIMIZATION_ANALYSIS.md) - Detailed analysis and recommendations
- [GPU_IMPLEMENTATION_STATUS.md](GPU_IMPLEMENTATION_STATUS.md) - Current implementation status
- [GPU_PERFORMANCE_ANALYSIS.md](GPU_PERFORMANCE_ANALYSIS.md) - Historical performance data

## Contributing Profiling Data

When reporting performance:

1. Include full profiling output
2. Specify GPU model and driver version
3. Note system specs (CPU, RAM)
4. Indicate input format and parameters
5. Run at least 3 times for consistency

Example:
```
GPU: NVIDIA RTX 4070 Ti (Driver 537.13)
CPU: AMD Ryzen 9 5900X
RAM: 32GB DDR4-3600
Input: PAL VHS, 40 MSPS, 50 frames
Command: python decode.py vhs --system PAL --gpu --gpu-profile --length 50 input.u8 output
Results: 2.13 FPS, see attached profile output
```
