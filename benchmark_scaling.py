#!/usr/bin/env python3
"""
VHS-Decode GPU Scaling Benchmark
Tests different thread counts to understand CPU scaling and GPU performance
"""

import subprocess
import time
import json
import os
import sys
from pathlib import Path

# Test configurations
SAMPLES = [
    {
        "name": "out2",
        "file": "sample_data/out2.u8",
        "frames": 11,
        "frequency": "20",
        "decoder": "vhs",
        "system": "PAL"
    },
]

THREAD_COUNTS = [1, 6]

def run_decode(sample_file, output, frames, threads=5, use_gpu=False, frequency="40", decoder="vhs", system="PAL"):
    """Run vhs-decode and measure time. Returns (elapsed_time, success, error_msg)."""
    cmd = [
        sys.executable, "decode.py", decoder,
        "--system", system,
        "-f", frequency,
        "--threads", str(threads),
        "--overwrite",
        "--length", str(frames),
        sample_file,
        output
    ]
    
    if use_gpu:
        cmd.insert(7, "--gpu")
    
    start = time.time()
    print(cmd)
    result = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - start
    
    # Validate output files exist and have data
    tbc_file = Path(f"{output}.tbc")
    json_file = Path(f"{output}.tbc.json")
    
    success = False
    error_msg = ""
    
    if result.returncode != 0:
        error_msg = f"Decoder exited with code {result.returncode}"
        if result.stderr:
            error_msg += f". Stderr: {result.stderr[:200]}"
    elif not tbc_file.exists():
        error_msg = f"Output file {output}.tbc not created"
    elif tbc_file.stat().st_size == 0:
        error_msg = f"Output file {output}.tbc is empty (0 bytes)"
    elif not json_file.exists():
        error_msg = f"JSON metadata file {output}.tbc.json not created"
    elif json_file.stat().st_size == 0:
        error_msg = f"JSON metadata file is empty"
    else:
        # Validate JSON is parseable
        try:
            with open(json_file) as f:
                json.load(f)
            success = True
        except json.JSONDecodeError as e:
            error_msg = f"Invalid JSON in metadata: {str(e)[:100]}"
    
    return elapsed, success, error_msg

def main():
    print("\n" + "="*70)
    print("VHS-Decode GPU Scaling Benchmark")
    print("="*70 + "\n")
    
    results = []
    
    for sample in SAMPLES:
        print(f"Sample: {sample['name']}")
        file_size = os.path.getsize(sample['file']) / (1024**3)
        print(f"  Size: {file_size:.2f} GB")
        print(f"  Testing {sample['frames']} frames with different thread counts...\n")
        
        # Test different thread counts for CPU
        cpu_results = {}
        for threads in THREAD_COUNTS:
            print(f"  [CPU {threads}T] Processing...", end=" ", flush=True)
            cpu_time, cpu_success, cpu_error = run_decode(
                sample['file'], 
                f"bench_cpu_t{threads}",
                sample['frames'],
                threads=threads,
                use_gpu=False,
                frequency=sample['frequency'],
                decoder=sample['decoder'],
                system=sample['system']
            )
            if not cpu_success:
                print(f"FAILED! {cpu_error}")
                return 1
            cpu_results[threads] = cpu_time
            fps = sample['frames'] / cpu_time
            print(f"✓ {cpu_time:.2f}s ({fps:.2f} FPS)")
        
        print()
        
        # Test GPU with different thread counts
        gpu_results = {}
        for threads in THREAD_COUNTS:
            print(f"  [GPU {threads}T] Processing...", end=" ", flush=True)
            gpu_time, gpu_success, gpu_error = run_decode(
                sample['file'],
                f"bench_gpu_t{threads}", 
                sample['frames'],
                threads=threads,
                use_gpu=True,
                frequency=sample['frequency'],
                decoder=sample['decoder'],
                system=sample['system']
            )
            if not gpu_success:
                print(f"FAILED! {gpu_error}")
                return 1
            gpu_results[threads] = gpu_time
            fps = sample['frames'] / gpu_time
            print(f"✓ {gpu_time:.2f}s ({fps:.2f} FPS)")
        
        print()
        
        # Calculate speedups
        for threads in THREAD_COUNTS:
            speedup = cpu_results[threads] / gpu_results[threads]
            result = {
                "sample": sample['name'],
                "threads": threads,
                "frames": sample['frames'],
                "cpu_time_s": round(cpu_results[threads], 2),
                "gpu_time_s": round(gpu_results[threads], 2),
                "speedup": round(speedup, 2),
                "cpu_fps": round(sample['frames'] / cpu_results[threads], 2),
                "gpu_fps": round(sample['frames'] / gpu_results[threads], 2)
            }
            results.append(result)
        
        # Cleanup
        for threads in THREAD_COUNTS:
            for pattern in [f"bench_cpu_t{threads}*", f"bench_gpu_t{threads}*"]:
                for file in Path(".").glob(pattern):
                    file.unlink(missing_ok=True)
    
    # Display results
    print("\n" + "="*70)
    print("Scaling Analysis")
    print("="*70 + "\n")
    
    print("CPU Scaling:")
    print(f"{'Threads':<10} {'Time(s)':>10} {'FPS':>10} {'Scaling':>10}")
    print("-" * 40)
    baseline_cpu_time = next(r['cpu_time_s'] for r in results if r['threads'] == 1)
    for r in results:
        if r['threads'] in THREAD_COUNTS:
            scaling = baseline_cpu_time / r['cpu_time_s']
            print(f"{r['threads']:<10} {r['cpu_time_s']:>10.2f} {r['cpu_fps']:>10.2f} {scaling:>10.2f}x")
    
    print("\nGPU Scaling:")
    print(f"{'Threads':<10} {'Time(s)':>10} {'FPS':>10} {'Scaling':>10}")
    print("-" * 40)
    baseline_gpu_time = next(r['gpu_time_s'] for r in results if r['threads'] == 1)
    for r in results:
        if r['threads'] in THREAD_COUNTS:
            scaling = baseline_gpu_time / r['gpu_time_s']
            print(f"{r['threads']:<10} {r['gpu_time_s']:>10.2f} {r['gpu_fps']:>10.2f} {scaling:>10.2f}x")
    
    print("\nCPU vs GPU Speedup:")
    print(f"{'Threads':<10} {'CPU(s)':>10} {'GPU(s)':>10} {'Speedup':>10}")
    print("-" * 40)
    for r in results:
        print(f"{r['threads']:<10} {r['cpu_time_s']:>10.2f} {r['gpu_time_s']:>10.2f} {r['speedup']:>10.2f}x")
    
    # Analysis
    print("\n" + "="*70)
    print("Analysis")
    print("="*70)
    
    best_cpu_threads = min(results, key=lambda x: x['cpu_time_s'])
    best_gpu_threads = min(results, key=lambda x: x['gpu_time_s'])
    
    print(f"\nBest CPU Performance: {best_cpu_threads['threads']} threads")
    print(f"  Time: {best_cpu_threads['cpu_time_s']:.2f}s ({best_cpu_threads['cpu_fps']:.2f} FPS)")
    
    print(f"\nBest GPU Performance: {best_gpu_threads['threads']} threads")
    print(f"  Time: {best_gpu_threads['gpu_time_s']:.2f}s ({best_gpu_threads['gpu_fps']:.2f} FPS)")
    
    optimal_speedup = best_cpu_threads['cpu_time_s'] / best_gpu_threads['gpu_time_s']
    print(f"\nOptimal GPU Speedup: {optimal_speedup:.2f}x")
    
    if optimal_speedup < 1.0:
        print(f"\n⚠️  GPU is still {1/optimal_speedup:.2f}x SLOWER than CPU")
        print("   Root cause: CPU↔GPU transfer overhead (17 transfers/block)")
        print("   Solution: Phase 2 - keep entire pipeline on GPU")
    else:
        print(f"\n✅ GPU is {optimal_speedup:.2f}x FASTER than CPU")
    
    # Save results
    with open("scaling_benchmark_results.json", "w") as f:
        json.dump(results, f, indent=2)
    
    print("\nResults saved to scaling_benchmark_results.json")
    return 0

if __name__ == "__main__":
    exit(main())
