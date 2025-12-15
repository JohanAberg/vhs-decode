"""
Benchmark script to validate GPU optimization improvements.

Compares performance before and after optimizations.
"""

import time
import numpy as np
import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent))

try:
    import cupy as cp
    GPU_AVAILABLE = True
except ImportError:
    print("ERROR: CuPy not available - benchmarking requires GPU")
    sys.exit(1)

from vhsdecode.gpu_demod import unwrap_hilbert_gpu


def benchmark_unwrap_hilbert(size=131072, iterations=100):
    """Benchmark unwrap_hilbert_gpu performance."""
    print(f"\nBenchmarking unwrap_hilbert_gpu")
    print(f"Block size: {size:,} samples")
    print(f"Iterations: {iterations}")
    print("-" * 60)
    
    # Generate test data
    t = np.linspace(0, 1, size)
    # Simulate FM-modulated signal
    carrier_freq = 5e6
    mod_freq = 1e3
    deviation = 500e3
    signal = np.exp(1j * 2 * np.pi * (carrier_freq * t + (deviation/mod_freq) * np.sin(2 * np.pi * mod_freq * t)))
    
    # Transfer to GPU
    signal_gpu = cp.asarray(signal)
    freq_hz = 40e6
    
    # Warm-up
    for _ in range(5):
        result = unwrap_hilbert_gpu(signal_gpu, freq_hz)
        cp.cuda.Stream.null.synchronize()
    
    # Benchmark
    times = []
    for i in range(iterations):
        cp.cuda.Stream.null.synchronize()
        start = time.perf_counter()
        
        result = unwrap_hilbert_gpu(signal_gpu, freq_hz)
        
        cp.cuda.Stream.null.synchronize()
        elapsed = time.perf_counter() - start
        times.append(elapsed)
        
        if (i + 1) % 20 == 0:
            print(f"  Progress: {i+1}/{iterations}", end='\r')
    
    print()  # New line after progress
    
    # Statistics
    times_ms = np.array(times) * 1000
    mean_ms = np.mean(times_ms)
    std_ms = np.std(times_ms)
    min_ms = np.min(times_ms)
    max_ms = np.max(times_ms)
    median_ms = np.median(times_ms)
    
    print(f"\nResults:")
    print(f"  Mean:   {mean_ms:.3f} ms ± {std_ms:.3f} ms")
    print(f"  Median: {median_ms:.3f} ms")
    print(f"  Min:    {min_ms:.3f} ms")
    print(f"  Max:    {max_ms:.3f} ms")
    print(f"  Total:  {sum(times_ms):.2f} ms for {iterations} iterations")
    
    # Verify correctness
    result_cpu = cp.asnumpy(result)
    print(f"\nVerification:")
    print(f"  Output range: [{result_cpu.min():.2f}, {result_cpu.max():.2f}] Hz")
    print(f"  Mean freq: {result_cpu.mean():.2f} Hz")
    
    return mean_ms, std_ms


def benchmark_full_decode(length=50):
    """Benchmark full decode with profiling."""
    import subprocess
    
    print(f"\n{'='*60}")
    print(f"Full Decode Benchmark ({length} frames)")
    print(f"{'='*60}\n")
    
    cmd = [
        sys.executable,
        "decode.py",
        "vhs",
        "--system", "PAL",
        "-f", "40",
        "--threads", "1",
        "--overwrite",
        f"--length", str(length),
        "--gpu",
        "--gpu-profile",
        "sample_data/out2.u8",
        "bench_output"
    ]
    
    print(f"Running: {' '.join(cmd)}\n")
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    # Extract profiling summary from output
    output = result.stdout + result.stderr
    if "GPU PROFILING SUMMARY" in output:
        # Print only the profiling summary
        lines = output.split('\n')
        in_summary = False
        for line in lines:
            if "GPU PROFILING SUMMARY" in line:
                in_summary = True
            if in_summary:
                print(line)
                if line.startswith("===") and "SUMMARY" not in line:
                    break
    
    # Extract FPS
    for line in output.split('\n'):
        if "FPS post-setup" in line:
            print(f"\n{line}")
    
    return result.returncode == 0


def main():
    print("="*60)
    print("GPU OPTIMIZATION BENCHMARK")
    print("="*60)
    
    # GPU Info
    device = cp.cuda.Device()
    props = cp.cuda.runtime.getDeviceProperties(device.id)
    print(f"\nGPU: {props['name'].decode()}")
    print(f"Compute Capability: {props['major']}.{props['minor']}")
    print(f"Memory: {props['totalGlobalMem'] / (1024**3):.2f} GB")
    
    # Benchmark individual function
    print("\n" + "="*60)
    print("MICRO-BENCHMARK: unwrap_hilbert_gpu")
    print("="*60)
    
    mean_ms, std_ms = benchmark_unwrap_hilbert(size=131072, iterations=100)
    
    print(f"\n{'='*60}")
    print(f"BASELINE (from profiling): 2.607 ms/block")
    print(f"OPTIMIZED:                 {mean_ms:.3f} ms/block")
    
    if mean_ms < 2.607:
        speedup = 2.607 / mean_ms
        improvement_pct = ((2.607 - mean_ms) / 2.607) * 100
        print(f"IMPROVEMENT:               {speedup:.2f}x speedup ({improvement_pct:.1f}% faster)")
        print(f"✅ OPTIMIZATION SUCCESSFUL!")
    else:
        slowdown = mean_ms / 2.607
        print(f"⚠️ SLOWER:                  {slowdown:.2f}x ({mean_ms - 2.607:.3f} ms worse)")
    print(f"{'='*60}")
    
    # Full decode benchmark
    print("\n" + "="*60)
    print("FULL DECODE BENCHMARK")
    print("="*60)
    
    success = benchmark_full_decode(length=50)
    
    if not success:
        print("\n⚠️ Full decode benchmark failed")
        return 1
    
    print("\n" + "="*60)
    print("BENCHMARK COMPLETE")
    print("="*60)
    print("\nNext steps:")
    print("1. Review profiling output above")
    print("2. Check if FM demodulation % decreased")
    print("3. Verify overall FPS improved")
    print("4. If successful, commit changes")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
