# GPU Phase 2 Benchmark Instructions

**Purpose:** Validate Phase 2 GPU optimization and measure actual performance improvements.

---

## Prerequisites

### Hardware Requirements
- NVIDIA GPU with CUDA support (Compute Capability 6.0+)
- 4GB+ VRAM (8GB+ recommended)
- CPU with 4+ cores for comparison

### Software Requirements
```bash
# 1. CUDA Toolkit (if not already installed)
# Download from: https://developer.nvidia.com/cuda-downloads
# Recommended: CUDA 12.6

# 2. Verify CUDA installation
nvidia-smi
nvcc --version

# 3. Install VHS-Decode with GPU support
pip install -e .
pip install cupy-cuda12x  # or cupy-cuda11x for CUDA 11.x

# 4. Verify GPU is detected
python -c "import cupy as cp; print(f'GPU: {cp.cuda.Device().name}')"
```

---

## Benchmark Suite

### 1. Quick Validation Test

**Purpose:** Verify Phase 2 implementation loads and runs without errors.

```bash
cd /home/runner/work/vhs-decode/vhs-decode
python tests/test_gpu_quick.py
```

**Expected Output:**
```
✓ GPU available
✓ CuPy version: 13.x.x
✓ CUDA version: 12.x
✓ GPU: <Your GPU Name>
✓ Basic GPU operations working
```

---

### 2. Phase 2 Unit Tests

**Purpose:** Validate correctness of GPU operations vs CPU equivalents.

```bash
pytest tests/test_gpu_phase2.py -v
```

**Expected Results:**
- 13 tests should PASS
- GPU operations match CPU output within tolerance (rtol=1e-4)
- No errors or warnings

**Critical Tests:**
- `test_unwrap_hilbert_vs_cpu` - FM demod correctness
- `test_replace_spikes_vs_cpu` - Spike replacement correctness
- `test_envelope_filter_basic` - Envelope filtering
- `test_nonlinear_deemphasis_basic` - NL deemphasis

---

### 3. Transfer Count Validation

**Purpose:** Verify that Phase 2 reduces transfers from 17 to 2-3 per block.

**Method:** Add transfer counting to the code temporarily:

```python
# Add to vhsdecode/gpu_utils.py
transfer_count = 0

def transfer_to_gpu(data):
    global transfer_count
    transfer_count += 1
    # ... existing code ...

def transfer_from_gpu(data):
    global transfer_count
    transfer_count += 1
    # ... existing code ...
```

Then decode a short sample and check the count:
```bash
python decode.py vhs --gpu --length 10 sample.u8 test_out
# Check logs for transfer_count
```

**Expected Results:**
- **Phase 1 (with `--no-gpu-optimize-transfers`):** ~170 transfers (17 per block × 10 blocks)
- **Phase 2 (default):** ~20-30 transfers (2-3 per block × 10 blocks)

---

### 4. Performance Benchmark - Thread Scaling

**Purpose:** Measure Phase 2 performance vs Phase 1 vs CPU with different thread counts.

```bash
python benchmark_scaling.py
```

This will test:
- CPU with 1, 2, 4, 8 threads
- GPU Phase 1 with 1, 2, 4, 8 threads (--no-gpu-optimize-transfers)
- GPU Phase 2 with 1, 2, 4, 8 threads (default)

**Expected Results:**

| Configuration | Threads | FPS | Speedup vs CPU |
|---------------|---------|-----|----------------|
| CPU | 8 | 40.6 | 1.0x |
| GPU Phase 1 | 4 | 35.3 | 0.87x |
| **GPU Phase 2** | **4** | **122-243** | **3-6x** ✅ |

**Key Metrics to Verify:**
- GPU Phase 2 should be 3-6x faster than CPU baseline
- GPU Phase 2 should be 3.5-7x faster than GPU Phase 1
- Optimal thread count for GPU should be 4 (as before)

---

### 5. Performance Benchmark - Component-Level

**Purpose:** Measure speedup of individual GPU operations.

```bash
pytest tests/test_gpu_benchmark.py -v --benchmark-only
```

**Expected Component Speedups:**

| Operation | Phase 1 | Phase 2 | Target |
|-----------|---------|---------|--------|
| FFT 32K | 6.5x | 6.5x | ✅ Same |
| FFT 64K | 11.8x | 11.8x | ✅ Same |
| FM Demod | CPU | **3-5x** | ✅ New |
| Envelope Filter | CPU | **4-6x** | ✅ New |
| Spike Replace | CPU | **2-3x** | ✅ New |
| NL Deemphasis | Mixed | **3-4x** | ✅ New |

---

### 6. Real-World Decode Test

**Purpose:** Validate Phase 2 with actual VHS captures.

```bash
# Phase 2 (default)
time vhs-decode --gpu --threads 4 --length 100 sample.u8 output_p2
# Record time: ___ seconds

# Phase 1 (for comparison)
time vhs-decode --gpu --no-gpu-optimize-transfers --threads 4 --length 100 sample.u8 output_p1
# Record time: ___ seconds

# CPU (for baseline)
time vhs-decode --threads 8 --length 100 sample.u8 output_cpu
# Record time: ___ seconds
```

**Calculate Speedups:**
```
Phase 2 vs CPU = CPU_time / Phase2_time
Phase 2 vs Phase 1 = Phase1_time / Phase2_time
```

**Expected:**
- Phase 2 vs CPU: 3-6x speedup
- Phase 2 vs Phase 1: 3.5-7x speedup

---

### 7. Output Correctness Validation

**Purpose:** Verify GPU output matches CPU output.

```bash
# Decode same sample with CPU and GPU
vhs-decode --threads 8 --length 100 sample.u8 output_cpu
vhs-decode --gpu --threads 4 --length 100 sample.u8 output_gpu

# Compare TBC files
python -c "
import numpy as np
cpu = np.fromfile('output_cpu.tbc', dtype=np.uint16)
gpu = np.fromfile('output_gpu.tbc', dtype=np.uint16)
diff = np.abs(cpu.astype(float) - gpu.astype(float))
max_diff = np.max(diff)
mean_diff = np.mean(diff)
print(f'Max difference: {max_diff}')
print(f'Mean difference: {mean_diff}')
print(f'Match: {max_diff < 10}')  # Should be < 10 for 16-bit values
"
```

**Expected:**
- Max difference: < 10 (less than 1 bit difference on 16-bit scale)
- Mean difference: < 2
- Visual inspection: No visible artifacts

---

### 8. Memory Usage Test

**Purpose:** Verify GPU memory usage is within acceptable limits.

```bash
# Monitor GPU memory during decode
nvidia-smi -l 1 &  # Start monitoring
MONITOR_PID=$!

vhs-decode --gpu --threads 4 --length 1000 sample.u8 output

kill $MONITOR_PID
```

**Expected:**
- Peak VRAM usage: < 4GB (target minimum GPU)
- Stable memory (no leaks)
- No out-of-memory errors

---

## Benchmark Results Template

Record your results here:

```markdown
## Benchmark Results

**Hardware:**
- GPU: _________________
- VRAM: _________________
- CPU: _________________
- RAM: _________________

**Software:**
- CUDA: _________________
- CuPy: _________________
- VHS-Decode: Phase 2

### Performance Results

| Test | CPU (8T) | GPU P1 (4T) | GPU P2 (4T) | Speedup |
|------|----------|-------------|-------------|---------|
| 100 frames | ___ s | ___ s | ___ s | ___x |
| Thread scaling | 40.6 FPS | 35.3 FPS | ___ FPS | ___x |

### Component Benchmarks

| Operation | CPU Time | GPU P2 Time | Speedup |
|-----------|----------|-------------|---------|
| FM Demod | ___ ms | ___ ms | ___x |
| Envelope Filter | ___ ms | ___ ms | ___x |
| Spike Replace | ___ ms | ___ ms | ___x |

### Transfer Count

- Phase 1: ___ transfers
- Phase 2: ___ transfers
- Reduction: ___%

### Correctness

- Max pixel difference: ___
- Mean pixel difference: ___
- Visual quality: Pass/Fail

### Conclusion

Phase 2 performance: ___x speedup over CPU (Target: 3-6x)
Status: PASS/FAIL
```

---

## Troubleshooting

### GPU Not Detected
```bash
# Check CUDA installation
nvidia-smi
nvcc --version

# Reinstall CuPy for correct CUDA version
pip uninstall cupy
pip install cupy-cuda12x  # or cupy-cuda11x
```

### Out of Memory Errors
```bash
# Use fewer threads
vhs-decode --gpu --threads 2 ...

# Or disable optimization temporarily
vhs-decode --gpu --no-gpu-optimize-transfers ...
```

### Performance Below Target
1. Check GPU utilization: `nvidia-smi -l 1`
2. Verify CUDA version matches CuPy
3. Check for thermal throttling
4. Try different thread counts (2-8)

---

## Expected Timeline

- **Quick validation:** 5 minutes
- **Unit tests:** 10 minutes
- **Thread scaling benchmark:** 30 minutes
- **Component benchmarks:** 15 minutes
- **Real-world test:** 20 minutes
- **Total:** ~1.5 hours for complete validation

---

## Reporting Results

Please report benchmark results with:
1. Hardware specs (GPU model, VRAM, CPU)
2. Performance numbers (FPS, speedup vs CPU)
3. Transfer count validation
4. Output correctness check
5. Any issues encountered

Share results in GitHub issue or PR comments.

---

**Note:** These benchmarks validate that Phase 2 achieves the target 3-6x speedup over CPU by eliminating the transfer bottleneck. Results below 3x indicate an issue that needs investigation.
