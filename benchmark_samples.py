#!/usr/bin/env python3
"""
VHS-Decode GPU Benchmark Script
Compares CPU vs GPU performance on sample files
"""

import subprocess
import time
import json
import os
from pathlib import Path

# Test configurations
SAMPLES = [
    {"name": "sony_short_40M_8b", "file": "sample_data/sony_short_40M_8b.u8", "frames": 100},
    {"name": "sony_nichols_new_cable_v01", "file": "sample_data/sony_nichols_new_cable_v01.u8", "frames": 100},
]

def run_decode(sample_file, output, frames, use_gpu=False):
    """Run vhs-decode and measure time."""
    cmd = [
        "python", "decode.py", "vhs",
        "--system", "PAL",
        "-f", "40",
        "--overwrite",
        "--length", str(frames),
        sample_file,
        output
    ]
    
    if use_gpu:
        cmd.insert(7, "--gpu")
    
    start = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - start
    
    return elapsed, result.returncode == 0

def main():
    print("\n" + "="*60)
    print("VHS-Decode GPU Benchmark")
    print("="*60 + "\n")
    
    results = []
    
    for sample in SAMPLES:
        print(f"Sample: {sample['name']}")
        
        file_size = os.path.getsize(sample['file']) / (1024**3)
        print(f"  Size: {file_size:.2f} GB")
        print(f"  Testing {sample['frames']} frames...\n")
        
        # CPU Test
        print("  [CPU] Processing...", end=" ", flush=True)
        cpu_time, cpu_success = run_decode(
            sample['file'], 
            f"bench_{sample['name']}_cpu",
            sample['frames'],
            use_gpu=False
        )
        print(f"Done! {cpu_time:.2f}s ({sample['frames']/cpu_time:.2f} FPS)")
        
        # GPU Test
        print("  [GPU] Processing...", end=" ", flush=True)
        gpu_time, gpu_success = run_decode(
            sample['file'],
            f"bench_{sample['name']}_gpu", 
            sample['frames'],
            use_gpu=True
        )
        print(f"Done! {gpu_time:.2f}s ({sample['frames']/gpu_time:.2f} FPS)")
        
        # Calculate speedup
        speedup = cpu_time / gpu_time if gpu_time > 0 else 0
        
        result = {
            "sample": sample['name'],
            "size_gb": round(file_size, 2),
            "frames": sample['frames'],
            "cpu_time_s": round(cpu_time, 2),
            "gpu_time_s": round(gpu_time, 2),
            "speedup": round(speedup, 2),
            "cpu_fps": round(sample['frames'] / cpu_time, 2),
            "gpu_fps": round(sample['frames'] / gpu_time, 2),
            "cpu_success": cpu_success,
            "gpu_success": gpu_success
        }
        results.append(result)
        
        print(f"  Speedup: {speedup:.2f}x")
        print()
        
        # Cleanup
        for pattern in [f"bench_{sample['name']}_cpu*", f"bench_{sample['name']}_gpu*"]:
            for file in Path(".").glob(pattern):
                file.unlink(missing_ok=True)
    
    # Display results
    print("\n" + "="*60)
    print("Benchmark Results")
    print("="*60 + "\n")
    
    print(f"{'Sample':<30} {'Frames':>7} {'CPU(s)':>8} {'GPU(s)':>8} {'Speedup':>8}")
    print("-" * 60)
    for r in results:
        print(f"{r['sample']:<30} {r['frames']:>7} {r['cpu_time_s']:>8.2f} {r['gpu_time_s']:>8.2f} {r['speedup']:>8.2f}x")
    
    avg_speedup = sum(r['speedup'] for r in results) / len(results)
    print("-" * 60)
    print(f"{'Average Speedup':<30} {'':<7} {'':<8} {'':<8} {avg_speedup:>8.2f}x\n")
    
    # Save results
    with open("benchmark_results.json", "w") as f:
        json.dump(results, f, indent=2)
    
    print("Results saved to benchmark_results.json")

if __name__ == "__main__":
    main()
