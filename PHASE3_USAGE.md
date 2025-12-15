# Phase 3: Batch Processing Usage Guide

**Date:** December 15, 2025  
**Status:** Phase 3A Complete - Ready for Testing  
**Feature:** GPU Batch Processing

---

## Quick Start

### Basic GPU Decode with Batch Processing

```bash
# Enable GPU with batch processing
python decode.py vhs --system PAL --gpu --batch-processing input.u8 output

# With custom batch size
python decode.py vhs --system PAL --gpu --batch-processing --batch-size 20 input.u8 output
```

### Command-Line Options

**`--batch-processing`** (Experimental)
- Enables GPU batch processing mode
- Processes multiple blocks together to reduce PCIe transfer overhead
- Default: disabled (for stability)
- Recommended for: Decodes where maximum performance is needed

**`--batch-size SIZE`** (Advanced)
- Number of blocks to process per batch
- Range: 10-100
- Default: auto-detected based on available VRAM
- Larger values: Lower overhead, higher VRAM usage
- Smaller values: Higher overhead, lower VRAM usage

**Examples:**
```bash
# Auto batch size (recommended)
python decode.py vhs --gpu --batch-processing input.u8 output

# Small batch for testing
python decode.py vhs --gpu --batch-processing --batch-size 10 input.u8 output

# Large batch for maximum performance
python decode.py vhs --gpu --batch-processing --batch-size 50 input.u8 output

# Disable batch processing (process blocks individually)
python decode.py vhs --gpu input.u8 output  # batch-processing off by default
```

---

## Expected Performance

### Without Batch Processing (Current Default)

```bash
python decode.py vhs --system PAL --gpu input.u8 output
```

**Performance:**
- FPS: ~2.49
- Transfer overhead: 20.9%
- Worker threads: 6
- GPU utilization: Low (idle between blocks)

### With Batch Processing (Phase 3A)

```bash
python decode.py vhs --system PAL --gpu --batch-processing input.u8 output
```

**Performance:**
- FPS: ~4.2 (target)
- Transfer overhead: 5-10% (↓60-75%)
- Worker threads: 1
- GPU utilization: High (batch processing)

**Speedup:** +69% over non-batch mode

---

## How It Works

### Individual Block Processing (Default)

```
Worker Thread 1: Block 1 → Transfer to GPU → Process → Transfer to CPU
Worker Thread 2: Block 2 → Transfer to GPU → Process → Transfer to CPU
Worker Thread 3: Block 3 → Transfer to GPU → Process → Transfer to CPU
...

PCIe Overhead = 0.5ms × 2 × 2710 blocks = 2.7 seconds (20.9%)
```

### Batch Processing (Phase 3A)

```
Batch Worker: Blocks 1-10 → Transfer to GPU → Process all → Transfer to CPU
              Blocks 11-20 → Transfer to GPU → Process all → Transfer to CPU
              ...

PCIe Overhead = 0.5ms × 2 × 271 batches = 0.27 seconds (5-10%)
```

**Key Difference:** Transfer latency amortized over entire batch, not per block.

---

## Batch Size Selection

### Factors to Consider

1. **Available VRAM**
   - Small GPUs (4-6 GB): batch_size = 10-20
   - Medium GPUs (8-10 GB): batch_size = 20-50
   - Large GPUs (12+ GB): batch_size = 50-100

2. **Block Size**
   - Standard (32K-131K): Use auto-detection
   - Custom sizes: May need manual tuning

3. **Performance Goals**
   - Maximum speed: Larger batch size
   - Memory efficiency: Smaller batch size
   - Balance: Use auto-detection

### Auto-Detection Logic

```python
# Calculated in _setup_gpu()
target_vram_gb = min(mem_free_gb * 0.25, 3.0)
estimated_mb_per_block = 2.0
optimal_batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
optimal_batch_size = max(10, min(optimal_batch_size, 100))
```

**Examples:**
- 4 GB free VRAM → batch_size = 50
- 8 GB free VRAM → batch_size = 100 (capped)
- 12 GB free VRAM → batch_size = 100 (capped)

---

## Troubleshooting

### "Out of memory" Errors

**Symptom:**
```
ERROR: CUDA out of memory allocating batch
```

**Solution:**
```bash
# Reduce batch size
python decode.py vhs --gpu --batch-processing --batch-size 10 input.u8 output

# Or disable batch processing
python decode.py vhs --gpu input.u8 output
```

### Lower Performance Than Expected

**Symptom:** FPS lower than 4.0 with batch processing

**Possible Causes:**
1. Batch size too small
2. GPU being throttled (thermal/power)
3. CPU bottleneck in non-GPU operations

**Solutions:**
```bash
# Try larger batch size
python decode.py vhs --gpu --batch-processing --batch-size 50 input.u8 output

# Check GPU throttling
nvidia-smi -l 1  # Watch clocks and temp

# Enable profiling to identify bottleneck
python decode.py vhs --gpu --batch-processing --gpu-profile --length 50 input.u8 output
```

### Decode Failure or Corruption

**Symptom:** Output .tbc is corrupted or decode crashes

**Solution:**
```bash
# Disable batch processing and report bug
python decode.py vhs --gpu input.u8 output

# Or fall back to CPU
python decode.py vhs input.u8 output
```

**Note:** Batch processing is experimental. If you encounter issues, please report them with:
- System specs (GPU model, VRAM)
- Command used
- Error messages
- Sample file (if possible)

---

## Performance Measurement

### Basic Benchmark

```bash
# Test without batch processing
time python decode.py vhs --gpu --length 100 input.u8 output_nobatch

# Test with batch processing
time python decode.py vhs --gpu --batch-processing --length 100 input.u8 output_batch

# Compare
echo "Speedup: $(python -c 'import sys; print(float(sys.argv[1])/float(sys.argv[2]))' <time1> <time2>)x"
```

### With Profiling

```bash
# Get detailed breakdown
python decode.py vhs --gpu --batch-processing --gpu-profile --length 50 input.u8 output

# Look for:
# - Transfer CPU→GPU time (should be low)
# - Transfer GPU→CPU time (should be low)
# - Transfer overhead % (should be 5-10%)
```

### Batch Size Tuning

```bash
# Test different batch sizes
for size in 10 20 50 100; do
    echo "Testing batch_size=$size"
    time python decode.py vhs --gpu --batch-processing --batch-size $size --length 100 input.u8 output_$size
done

# Find optimal size
ls -lt output_*.log | head -1  # Fastest
```

---

## Migration Guide

### From CPU to GPU

```bash
# Before (CPU only)
python decode.py vhs input.u8 output

# After (GPU without batching)
python decode.py vhs --gpu input.u8 output

# After (GPU with batching - best performance)
python decode.py vhs --gpu --batch-processing input.u8 output
```

### From GPU Non-Batch to Batch

```bash
# Before (GPU, no batching)
python decode.py vhs --gpu input.u8 output
# Performance: ~2.49 FPS

# After (GPU with batching)
python decode.py vhs --gpu --batch-processing input.u8 output
# Expected: ~4.2 FPS (+69%)
```

---

## Compatibility

### Supported Formats
- ✅ VHS (all variants: SVHS, VHS-C, etc.)
- ✅ NTSC
- ✅ PAL
- ✅ All tape speeds (SP, LP, EP, etc.)

### Requirements
- GPU: NVIDIA with CUDA 11+
- VRAM: 4 GB minimum, 8+ GB recommended
- CuPy: Installed (`pip install cupy-cuda12x`)
- Python: 3.8+

### Limitations
- **Experimental feature:** May have stability issues
- **Single GPU only:** Multi-GPU not supported yet
- **Fixed batch size:** Cannot change during decode

---

## Future Enhancements

### Phase 3B (Planned)
- Dynamic batch size adjustment during decode
- Better prefetching and queue management
- Multi-GPU support
- Performance: +80-100% over CPU (vs current +69%)

### Phase 4 (Planned)
- Fused FM demodulation CUDA kernel
- Reduces FM demod overhead from 20.9% to 4.3%
- Performance: +97% over CPU

### Phase 5 (Planned)
- Async I/O loader integration
- CUDA streams for overlapped execution
- Performance: +221-382% over CPU (approaching 10× target)

---

## Examples

### Standard Decode

```bash
# PAL VHS with batch processing
python decode.py vhs --system PAL --gpu --batch-processing input.u8 output

# NTSC SVHS with batch processing
python decode.py vhs --system NTSC --tape-format SVHS --gpu --batch-processing input.u8 output
```

### High-Performance Decode

```bash
# Maximum performance settings
python decode.py vhs \
    --system PAL \
    --gpu \
    --batch-processing \
    --batch-size 50 \
    --gpu-optimize-transfers \
    input.u8 output
```

### Debug/Testing

```bash
# With profiling
python decode.py vhs --gpu --batch-processing --gpu-profile --length 50 input.u8 output

# Small test
python decode.py vhs --gpu --batch-processing --length 11 input.u8 test

# Fallback to non-batch
python decode.py vhs --gpu input.u8 output_safe
```

---

## Validation

After enabling batch processing, verify output quality:

```bash
# Decode with and without batching
python decode.py vhs --gpu --length 1000 input.u8 output_nobatch
python decode.py vhs --gpu --batch-processing --length 1000 input.u8 output_batch

# Compare outputs
python compare_tbc.py output_nobatch.tbc output_batch.tbc

# Should show minimal differences (within numerical precision)
```

---

## Support

**Issues:**
- Report bugs on GitHub issue tracker
- Include system specs and command used
- Attach relevant log files

**Performance Questions:**
- Check GPU_PROFILING_GUIDE.md for profiling instructions
- Review VRAM_OPTIMIZATION_GUIDE.md for tuning tips

**Documentation:**
- PHASE3_INTEGRATION_PLAN.md - Architecture details
- VRAM_OPTIMIZATION_SUMMARY.md - Overall project status
- GPU_OPTIMIZATION_SUGGESTIONS.md - Future optimizations

---

**Document Version:** 1.0  
**Last Updated:** December 15, 2025  
**Status:** Phase 3A Complete  
**Feature Status:** Experimental (ready for testing)
