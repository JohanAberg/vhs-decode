# GPU Acceleration Testing Guide

This guide explains how to test and validate the GPU acceleration implementation for VHS-Decode.

## Prerequisites

Before running GPU tests, ensure you have:

1. **VHS-Decode installed** with all dependencies:
   ```bash
   pip install -e .
   ```

2. **GPU dependencies** (if testing GPU):
   ```bash
   # For CUDA 11.x
   pip install cupy-cuda11x
   
   # For CUDA 12.x
   pip install cupy-cuda12x
   ```

3. **Testing dependencies**:
   ```bash
   pip install pytest pytest-benchmark
   ```

## Quick Validation

Run the quick validation script to check if GPU infrastructure is working:

```bash
python tests/test_gpu_quick.py
```

This will:
- Check if GPU utilities can be imported
- Detect available GPU(s)
- Verify basic GPU operations
- Test fallback to CPU when GPU unavailable

Expected output with GPU:
```
============================================================
GPU Infrastructure Quick Validation
============================================================

GPU Utils Import:
------------------------------------------------------------
✓ GPU utilities module imported successfully

GPU Detection:
------------------------------------------------------------

GPU Detection:
  GPU Available: True

  GPU Status:
GPU: NVIDIA GeForce RTX 3070
Compute Capability: (8, 6)
Total VRAM: 8.00 GB
Free VRAM: 7.85 GB
✓ GPU detection test passed
...
```

## Running Unit Tests

Unit tests verify that GPU implementations produce identical results to CPU:

```bash
# Run all GPU unit tests
pytest tests/test_gpu_unit.py -v

# Run specific test
pytest tests/test_gpu_unit.py::TestGPUFFT::test_fft_equivalence -v

# Skip tests if GPU not available (automatic)
pytest tests/test_gpu_unit.py -v
```

### Key Unit Tests

1. **FFT Equivalence** - Verifies GPU FFT matches CPU FFT exactly
2. **Filter Application** - Tests frequency-domain filtering
3. **Memory Management** - Checks for memory leaks
4. **Precision Tests** - Validates FP32 vs FP64 behavior

## Running Integration Tests

Integration tests verify the full decode pipeline:

```bash
# Run integration tests (requires test data)
pytest tests/test_gpu_integration.py -v

# Skip slow tests
pytest tests/test_gpu_integration.py -v -m "not slow"
```

**Note**: Integration tests require test captures in `tests/fixtures/gpu_validation/`. See `tests/fixtures/README.md` for how to obtain test data.

## Running Benchmark Tests

Benchmark tests measure GPU performance:

```bash
# Run all benchmarks
pytest tests/test_gpu_benchmark.py -v --benchmark-only

# Run specific benchmark group
pytest tests/test_gpu_benchmark.py::TestFFTBenchmark -v --benchmark-only

# Compare CPU vs GPU
pytest tests/test_gpu_benchmark.py -v --benchmark-only --benchmark-compare
```

### Benchmark Groups

- **fft** - FFT operations (32K, 64K samples)
- **filtering** - Frequency-domain filtering
- **hilbert** - Hilbert transform
- **complex** - Complex number operations
- **pipeline** - End-to-end demodulation simulation
- **transfer** - CPU<->GPU transfer overhead

## Performance Tracking

Track performance over time using the benchmark tracker:

```bash
# Run speedup tracking test
pytest tests/test_gpu_benchmark.py::test_speedup_tracking -v

# Generate performance report
python tests/benchmark_tracker.py
```

This creates:
- `benchmark_results.json` - Performance data
- `benchmark_report.md` - Formatted report

## Regression Tests

Regression tests ensure output hasn't degraded:

```bash
# Run regression tests (requires reference data)
pytest tests/test_gpu_regression.py -v
```

**Note**: These tests compare GPU output against known-good CPU baselines stored in `tests/fixtures/gpu_validation/`.

## Testing Without GPU

All tests gracefully skip when GPU is not available:

```bash
# These will skip GPU tests automatically
pytest tests/test_gpu_unit.py -v
pytest tests/test_gpu_integration.py -v
```

Output will show:
```
tests/test_gpu_unit.py::TestGPUUtilities::test_gpu_available SKIPPED (GPU not available)
```

## Continuous Integration

For CI/CD pipelines, use:

```yaml
# .github/workflows/gpu-tests.yml
- name: Run GPU tests
  run: |
    pytest tests/test_gpu_unit.py -v
    pytest tests/test_gpu_integration.py -v -m "not slow"
    pytest tests/test_gpu_benchmark.py -v --benchmark-only
```

## Manual Testing

Test GPU acceleration with real captures:

### 1. Basic Usage Test

```bash
# Decode with GPU
vhs-decode --gpu input.lds output_gpu

# Decode with CPU for comparison
vhs-decode input.lds output_cpu

# Compare outputs
ld-analyse output_gpu.tbc
ld-analyse output_cpu.tbc
```

### 2. Performance Measurement

```bash
# Time GPU decode
time vhs-decode --gpu input.lds output_gpu

# Time CPU decode
time vhs-decode input.lds output_cpu

# Calculate speedup
# Speedup = CPU_time / GPU_time
```

### 3. Visual Comparison

Use ld-analyse to visually compare outputs:

```bash
# Open both TBC files
ld-analyse output_gpu.tbc &
ld-analyse output_cpu.tbc &

# Check for:
# - Frame alignment
# - Color accuracy
# - Sync accuracy
# - Dropout detection
```

## Profiling GPU Performance

### Using NVIDIA Tools

#### nvprof (older CUDA)
```bash
nvprof python -m vhsdecode.main --gpu input.lds output
```

#### Nsight Systems (recommended)
```bash
nsys profile python -m vhsdecode.main --gpu input.lds output
```

#### Nsight Compute (detailed kernel analysis)
```bash
ncu --set full python -m vhsdecode.main --gpu input.lds output
```

### Memory Profiling

Check for memory leaks:

```bash
# Monitor VRAM usage during decode
nvidia-smi -l 1

# Run memory leak test
pytest tests/test_gpu_unit.py::TestGPUMemoryManagement::test_no_memory_leak -v
```

## Troubleshooting Tests

### Test Failures

1. **Import Errors**
   ```
   ModuleNotFoundError: No module named 'cupy'
   ```
   **Solution**: Install CuPy: `pip install cupy-cuda11x` or `cupy-cuda12x`

2. **GPU Not Detected**
   ```
   GPU Available: False
   ```
   **Solution**: 
   - Check CUDA installation: `nvcc --version`
   - Check GPU: `nvidia-smi`
   - Reinstall CuPy

3. **Out of Memory**
   ```
   cupy.cuda.memory.OutOfMemoryError
   ```
   **Solution**:
   - Close other GPU applications
   - Use smaller test data
   - Check VRAM: `nvidia-smi`

4. **Test Timeouts**
   ```
   TIMEOUT after X seconds
   ```
   **Solution**:
   - Increase timeout: `pytest --timeout=300`
   - Check if GPU is thermal throttling: `nvidia-smi`

### Test Data Issues

If integration/regression tests skip:
```
SKIPPED (Test data not available)
```

**Solution**: See `tests/fixtures/README.md` for obtaining test captures.

## Test Coverage

Check test coverage:

```bash
# Install coverage tool
pip install pytest-cov

# Run with coverage
pytest tests/test_gpu_*.py --cov=vhsdecode.gpu_utils --cov=vhsdecode.process_gpu

# Generate HTML report
pytest tests/test_gpu_*.py --cov=vhsdecode.gpu_utils --cov=vhsdecode.process_gpu --cov-report=html
```

## Acceptance Criteria

For GPU implementation to be considered complete:

| Test Type | Pass Criteria | Status |
|-----------|--------------|--------|
| Unit Tests | 100% pass | ✓ |
| Integration Tests | <0.1% output difference | Pending test data |
| Performance Tests | 2x+ speedup | To be measured |
| Regression Tests | Output matches baseline | Pending test data |
| Memory Tests | No leaks, <4GB VRAM | ✓ |

## Contributing

When adding new GPU features:

1. **Write tests first** (TDD approach)
2. **Verify CPU equivalence** (output must match within tolerance)
3. **Benchmark performance** (track speedup)
4. **Update this guide** (document new tests)

See `.aitk/instructions/gpu-implementation.instructions.md` for detailed development guidelines.

## References

- [pytest Documentation](https://docs.pytest.org/)
- [pytest-benchmark](https://pytest-benchmark.readthedocs.io/)
- [CuPy Testing Guide](https://docs.cupy.dev/en/stable/reference/testing.html)
- [NVIDIA Profiler User Guide](https://docs.nvidia.com/cuda/profiler-users-guide/)
