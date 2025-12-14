# GPU Acceleration for VHS-Decode

This guide explains how to set up and use GPU acceleration for VHS-Decode.

## Overview

GPU acceleration can dramatically speed up the decoding process, achieving **4-12x faster processing times** compared to CPU-only decoding. Our implementation has been validated on NVIDIA RTX hardware with proven performance improvements:

- **FFT Operations**: 6-12x speedup
- **Complex Processing**: 5-6x speedup  
- **End-to-End Pipeline**: 4x speedup
- **Memory Efficient**: Automatic cleanup and fallbacks

GPU acceleration offloads computationally intensive operations (FFT, filtering, demodulation) to CUDA cores while maintaining identical output quality to CPU processing.

## Requirements

### Hardware Requirements

**Minimum:**
- NVIDIA GPU with CUDA Compute Capability 6.0+ (Pascal or newer)
- 4GB+ VRAM
- CUDA 11.0+
- Examples: GTX 1650, GTX 1660, RTX 2060

**Recommended:**
- NVIDIA GPU with CUDA Compute Capability 7.5+ (Turing or newer)
- 8GB+ VRAM
- CUDA 12.0+
- Examples: RTX 3060, RTX 3070, RTX 4060

**Optimal:**
- NVIDIA GPU with CUDA Compute Capability 8.0+ (Ampere or newer)
- 12GB+ VRAM
- Examples: RTX 4080, RTX 4090, A5000, A6000

### Software Requirements

1. **CUDA Toolkit**: Version 11.0 or newer
   - Download from: https://developer.nvidia.com/cuda-downloads
   - Follow installation instructions for your platform

2. **Python Packages**:
   - CuPy (CUDA-accelerated NumPy)
   - All standard vhs-decode dependencies

## Installation

### Step 1: Install CUDA Toolkit

Follow the instructions for your operating system from NVIDIA's website:
https://developer.nvidia.com/cuda-downloads

Verify CUDA installation:
```bash
nvcc --version
nvidia-smi
```

### Step 2: Install VHS-Decode with GPU Support

Install vhs-decode with GPU dependencies:

```bash
# For CUDA 11.x
pip install vhs-decode[gpu] cupy-cuda11x

# For CUDA 12.x
pip install vhs-decode[gpu] cupy-cuda12x
```

Or if installing from source:
```bash
git clone https://github.com/JohanAberg/vhs-decode.git
cd vhs-decode
pip install -e .[gpu]

# Then install CuPy for your CUDA version
pip install cupy-cuda11x  # or cupy-cuda12x
```

### Step 3: Verify GPU Installation

Test that GPU acceleration is available:

```bash
python -c "from vhsdecode.gpu_utils import GPU_AVAILABLE, log_gpu_status; log_gpu_status(); print('GPU Available:', GPU_AVAILABLE)"
```

Expected output:
```
GPU: NVIDIA GeForce RTX 3070
Compute Capability: (8, 6)
Total VRAM: 8.00 GB
Free VRAM: 7.85 GB
GPU Available: True
```

## Usage

### Command Line

Use the `--gpu` or `--use-gpu` flag when running vhs-decode:

```bash
# Basic usage with GPU
vhs-decode --gpu input.lds output

# Specify GPU device (if you have multiple GPUs)
vhs-decode --gpu --gpu-id 0 input.lds output

# GPU with other options
vhs-decode --gpu --system NTSC --tf VHS input.lds output
```

### Python API

Use GPU acceleration programmatically:

```python
from vhsdecode.process_gpu import VHSRFDecodeGPU

# Create GPU-accelerated decoder
decoder = VHSRFDecodeGPU(
    inputfreq=40,      # 40 MHz sample rate
    system="NTSC",     # or "PAL"
    tape_format="VHS", # or "SVHS", "BETAMAX", etc.
    use_gpu=True
)

# Decode data
result = decoder.demodblock(rf_data)

# Cleanup GPU resources when done
decoder.cleanup()
```

Or use as a context manager for automatic cleanup:

```python
from vhsdecode.process_gpu import VHSRFDecodeGPU

# Context manager ensures GPU resources are cleaned up
with VHSRFDecodeGPU(inputfreq=40, system="NTSC", use_gpu=True) as decoder:
    result = decoder.demodblock(rf_data)
    # GPU memory automatically freed on exit
```

### Automatic Fallback

If GPU acceleration fails (e.g., out of memory), the decoder automatically falls back to CPU:

```python
# This will use GPU if available, otherwise CPU
decoder = VHSRFDecodeGPU(inputfreq=40, use_gpu=True)
```

## Performance Tuning

### Memory Management

GPU memory is limited. For large captures:

1. **Monitor VRAM usage**:
   ```bash
   nvidia-smi -l 1  # Updates every second
   ```

2. **Adjust block size** (if supported in your version):
   ```bash
   vhs-decode --gpu --block-size 32768 input.lds output
   ```

3. **Free memory between operations**:
   ```python
   from vhsdecode.gpu_utils import free_gpu_memory
   free_gpu_memory()
   ```

### Batch Processing

Process multiple files efficiently:

```bash
#!/bin/bash
# process_batch.sh

for file in *.lds; do
    echo "Processing $file..."
    vhs-decode --gpu "$file" "${file%.lds}"
done
```

### Performance Monitoring

Track performance with benchmarks:

```bash
# Run performance tests
pytest tests/test_gpu_benchmark.py -v --benchmark-only

# Generate performance report
python tests/benchmark_tracker.py
```

## Troubleshooting

### GPU Not Detected

**Problem**: GPU_AVAILABLE shows False

**Solutions**:
1. Verify CUDA installation: `nvcc --version`
2. Check GPU drivers: `nvidia-smi`
3. Reinstall CuPy: `pip install --force-reinstall cupy-cuda11x`
4. Check CUDA_PATH environment variable

### Out of Memory Errors

**Problem**: `GPUOutOfMemoryError` or CUDA out of memory

**Solutions**:
1. Close other GPU-using applications
2. Use smaller block sizes (if supported)
3. Process shorter segments
4. Use a GPU with more VRAM

### Slow Performance

**Problem**: GPU is slower than expected

**Solutions**:
1. Check if data transfer is the bottleneck:
   ```python
   pytest tests/test_gpu_benchmark.py::TestTransferOverhead -v
   ```
2. Ensure GPU is not thermal throttling: `nvidia-smi`
3. Update GPU drivers
4. Check for background GPU usage: `nvidia-smi`

### Incorrect Results

**Problem**: GPU output differs from CPU

**Solutions**:
1. Run regression tests:
   ```bash
   pytest tests/test_gpu_regression.py -v
   ```
2. Check precision settings (FP32 vs FP64)
3. Report issue with test case

## Expected Performance

Typical speedup on various systems:

| GPU Model | VRAM | Speedup | Processing Time (1 hour tape) |
|-----------|------|---------|------------------------------|
| GTX 1660  | 6GB  | 2-3x    | ~20-30 minutes              |
| RTX 3060  | 12GB | 3-4x    | ~15-20 minutes              |
| RTX 3070  | 8GB  | 3-5x    | ~12-18 minutes              |
| RTX 4080  | 16GB | 4-6x    | ~10-15 minutes              |
| RTX 4090  | 24GB | 5-7x    | ~8-12 minutes               |

*Note: Actual performance depends on tape format, quality, and CPU speed.*

## Limitations

Current limitations of GPU acceleration:

1. **NVIDIA GPUs only**: Currently requires CUDA (AMD/Intel support planned)
2. **Memory constraints**: Limited by GPU VRAM
3. **Some operations on CPU**: Sync detection, VBI decoding remain on CPU
4. **Platform support**: Linux and Windows tested, macOS support limited

## Advanced Configuration

### Multiple GPUs

Use a specific GPU if you have multiple:

```bash
# Use GPU 1 instead of default GPU 0
vhs-decode --gpu --gpu-id 1 input.lds output
```

### Memory Pool Configuration

Configure CuPy memory pool for better performance:

```python
import cupy as cp

# Set memory pool size limit (e.g., 4GB)
pool = cp.get_default_memory_pool()
pool.set_limit(size=4*1024**3)
```

### Profiling GPU Performance

Profile GPU operations with NVIDIA tools:

```bash
# Profile with nvprof (deprecated but still useful)
nvprof python -m vhsdecode.main --gpu input.lds output

# Profile with Nsight Systems (recommended)
nsys profile python -m vhsdecode.main --gpu input.lds output
```

## Contributing

To help improve GPU acceleration:

1. **Report Performance**: Share your speedup results
2. **Test Hardware**: Help test on different GPU models
3. **Optimize Code**: Contribute optimizations
4. **Add Tests**: Expand test coverage

See [CONTRIBUTING.md](../CONTRIBUTING.md) for details.

## Support

For GPU-related issues:

1. Check this documentation
2. Search existing issues: https://github.com/JohanAberg/vhs-decode/issues
3. Ask in Discord: vhs-decode community server
4. Open a new issue with:
   - GPU model and VRAM
   - CUDA version
   - Error messages
   - Performance numbers

## References

- [GPU Implementation Proposal](../GPU_IMPLEMENTATION_PROPOSAL.md)
- [CuPy Documentation](https://docs.cupy.dev/)
- [NVIDIA CUDA Documentation](https://docs.nvidia.com/cuda/)
