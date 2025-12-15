"""
GPU Performance Profiler for VHS-Decode

This script profiles GPU operations to identify optimization opportunities.
Measures:
- Time spent in each major operation (FFT, filtering, demod, etc.)
- Memory transfers between CPU and GPU
- GPU memory usage and allocation patterns
- Kernel execution times
"""

import argparse
import logging
import numpy as np
import sys
import time
from pathlib import Path
from collections import defaultdict

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(levelname)s: %(message)s'
)
logger = logging.getLogger(__name__)

try:
    import cupy as cp
    GPU_AVAILABLE = True
except ImportError:
    logger.error("CuPy not available - GPU profiling requires CuPy")
    sys.exit(1)


class GPUProfiler:
    """Profiles GPU operations with detailed timing and memory tracking."""
    
    def __init__(self, enabled=True):
        self.enabled = enabled
        self.timings = defaultdict(list)
        self.memory_transfers = defaultdict(lambda: {'count': 0, 'bytes': 0})
        self.gpu_memory_peak = 0
        self.active_timer = None
        self.timer_start = None
        
    def start_timer(self, name):
        """Start timing an operation."""
        if not self.enabled:
            return
        
        if self.active_timer:
            logger.warning(f"Timer '{self.active_timer}' not stopped before starting '{name}'")
            
        self.active_timer = name
        cp.cuda.Stream.null.synchronize()  # Ensure GPU operations complete
        self.timer_start = time.perf_counter()
        
    def stop_timer(self, name=None):
        """Stop timing an operation."""
        if not self.enabled:
            return
        
        cp.cuda.Stream.null.synchronize()  # Ensure GPU operations complete
        end_time = time.perf_counter()
        
        if name is None:
            name = self.active_timer
        
        if self.active_timer != name:
            logger.warning(f"Stopping timer '{name}' but '{self.active_timer}' is active")
        
        elapsed = end_time - self.timer_start
        self.timings[name].append(elapsed)
        self.active_timer = None
        
        return elapsed
        
    def record_transfer(self, direction, array):
        """Record a CPU↔GPU memory transfer."""
        if not self.enabled:
            return
        
        if isinstance(array, (np.ndarray, cp.ndarray)):
            nbytes = array.nbytes
        else:
            nbytes = 0
            
        self.memory_transfers[direction]['count'] += 1
        self.memory_transfers[direction]['bytes'] += nbytes
        
    def update_gpu_memory(self):
        """Update peak GPU memory usage."""
        if not self.enabled:
            return
        
        mempool = cp.get_default_memory_pool()
        used = mempool.used_bytes()
        self.gpu_memory_peak = max(self.gpu_memory_peak, used)
        
    def print_summary(self):
        """Print profiling summary."""
        if not self.enabled or not self.timings:
            return
        
        print("\n" + "="*80)
        print("GPU PROFILING SUMMARY")
        print("="*80)
        
        # Timing breakdown
        print("\n--- TIMING BREAKDOWN ---")
        print(f"{'Operation':<40} {'Count':>8} {'Total (s)':>12} {'Avg (ms)':>12} {'%':>8}")
        print("-" * 80)
        
        total_time = sum(sum(times) for times in self.timings.values())
        
        # Sort by total time descending
        sorted_timings = sorted(
            self.timings.items(),
            key=lambda x: sum(x[1]),
            reverse=True
        )
        
        for name, times in sorted_timings:
            count = len(times)
            total = sum(times)
            avg_ms = (total / count) * 1000
            pct = (total / total_time) * 100 if total_time > 0 else 0
            print(f"{name:<40} {count:>8} {total:>12.3f} {avg_ms:>12.3f} {pct:>7.1f}%")
        
        print("-" * 80)
        print(f"{'TOTAL':<40} {'':<8} {total_time:>12.3f} {'':<12} {'100.0':>7}%")
        
        # Memory transfers
        print("\n--- MEMORY TRANSFERS ---")
        total_transfer_bytes = 0
        for direction, stats in self.memory_transfers.items():
            count = stats['count']
            mb = stats['bytes'] / (1024 * 1024)
            total_transfer_bytes += stats['bytes']
            print(f"{direction:<20} {count:>8} transfers, {mb:>12.2f} MB")
        
        if total_transfer_bytes > 0:
            total_mb = total_transfer_bytes / (1024 * 1024)
            print(f"{'Total':<20} {'':<8} {total_mb:>24.2f} MB")
        
        # GPU memory
        print("\n--- GPU MEMORY ---")
        peak_mb = self.gpu_memory_peak / (1024 * 1024)
        print(f"Peak GPU memory usage: {peak_mb:.2f} MB")
        
        # Recommendations
        print("\n--- OPTIMIZATION RECOMMENDATIONS ---")
        self._print_recommendations()
        
        print("="*80 + "\n")
        
    def _print_recommendations(self):
        """Print optimization recommendations based on profiling data."""
        recommendations = []
        
        # Check for excessive transfers
        cpu_to_gpu = self.memory_transfers.get('CPU→GPU', {})
        gpu_to_cpu = self.memory_transfers.get('GPU→CPU', {})
        
        if cpu_to_gpu.get('count', 0) > 5:
            recommendations.append(
                f"• High number of CPU→GPU transfers ({cpu_to_gpu['count']}). "
                "Consider batching operations on GPU."
            )
        
        if gpu_to_cpu.get('count', 0) > 5:
            recommendations.append(
                f"• High number of GPU→CPU transfers ({gpu_to_cpu['count']}). "
                "Try to keep data on GPU longer."
            )
        
        # Check for dominant operations
        if self.timings:
            total_time = sum(sum(times) for times in self.timings.values())
            for name, times in self.timings.items():
                op_time = sum(times)
                pct = (op_time / total_time) * 100
                if pct > 30:
                    recommendations.append(
                        f"• '{name}' takes {pct:.1f}% of total time. "
                        "Focus optimization efforts here."
                    )
        
        if recommendations:
            for rec in recommendations:
                print(rec)
        else:
            print("• No critical issues detected. Performance looks good!")


def profile_demodblock_gpu(decoder, data, profiler):
    """Profile a single demodblock operation with detailed measurements."""
    
    # 1. Data transfer to GPU
    profiler.start_timer("1_CPU→GPU_transfer")
    data_gpu = cp.asarray(data[:decoder.blocklen])
    profiler.record_transfer("CPU→GPU", data_gpu)
    profiler.stop_timer()
    profiler.update_gpu_memory()
    
    # 2. FFT
    profiler.start_timer("2_FFT_forward")
    indata_fft_gpu = cp.fft.fft(data_gpu)
    profiler.stop_timer()
    profiler.update_gpu_memory()
    
    # 3. Filter application (RF + Hilbert)
    profiler.start_timer("3_RF_filter")
    rf_filter_gpu = decoder.Filters_gpu["RFVideo"]
    indata_fft_gpu *= rf_filter_gpu
    profiler.stop_timer()
    
    profiler.start_timer("4_Hilbert_filter")
    hilbert_filter_gpu = decoder.Filters_gpu["hilbert"]
    filtered_gpu = indata_fft_gpu * hilbert_filter_gpu
    profiler.stop_timer()
    profiler.update_gpu_memory()
    
    # 4. IFFT
    profiler.start_timer("5_IFFT")
    raw_filtered_gpu = cp.fft.ifft(filtered_gpu).real
    profiler.stop_timer()
    profiler.update_gpu_memory()
    
    # 5. Envelope calculation
    profiler.start_timer("6_Envelope_abs")
    cp.abs(raw_filtered_gpu, out=raw_filtered_gpu)
    profiler.stop_timer()
    
    profiler.start_timer("7_Envelope_roll")
    raw_env_gpu = cp.roll(raw_filtered_gpu, 4)
    profiler.stop_timer()
    
    # 6. Transfer back to CPU
    profiler.start_timer("8_GPU→CPU_transfer")
    raw_env = cp.asnumpy(raw_env_gpu)
    profiler.record_transfer("GPU→CPU", raw_env_gpu)
    profiler.stop_timer()
    
    # Cleanup
    del data_gpu, indata_fft_gpu, filtered_gpu, raw_filtered_gpu, raw_env_gpu
    profiler.update_gpu_memory()
    
    return raw_env


def profile_full_pipeline(rf_file, system="PAL", num_blocks=10):
    """Profile the full GPU decoding pipeline."""
    from vhsdecode.process_gpu import VHSRFDecodeGPU
    from vhsdecode.formats import RFParams, SysParams
    
    print(f"\nProfiling GPU pipeline: {rf_file}")
    print(f"System: {system}, Blocks: {num_blocks}")
    print("-" * 80)
    
    # Initialize decoder
    rf_params = RFParams(system=system, tape_format="VHS")
    sys_params = SysParams(system=system)
    
    decoder = VHSRFDecodeGPU(
        inputfreq=40,
        system=system,
        tape_format="VHS",
        rf_options={},
        extra_options={'use_gpu': True, 'gpu_phase2': True}
    )
    
    # Read RF data
    with open(rf_file, 'rb') as f:
        rf_data = np.frombuffer(f.read(), dtype=np.uint8).astype(np.float32)
    
    print(f"Loaded {len(rf_data):,} samples ({len(rf_data)/40e6:.2f} seconds)")
    
    # Create profiler
    profiler = GPUProfiler(enabled=True)
    
    # Profile multiple blocks
    blocklen = decoder.blocklen
    for block_idx in range(min(num_blocks, len(rf_data) // blocklen)):
        print(f"\rProcessing block {block_idx + 1}/{num_blocks}...", end='', flush=True)
        
        start_idx = block_idx * blocklen
        block_data = rf_data[start_idx:start_idx + blocklen + decoder.blockcut_end]
        
        if len(block_data) < blocklen:
            break
        
        # Profile this block
        profiler.start_timer(f"Block_{block_idx}_total")
        result = profile_demodblock_gpu(decoder, block_data, profiler)
        profiler.stop_timer()
    
    print("\rProcessing complete!              ")
    
    # Print results
    profiler.print_summary()
    
    # Additional GPU info
    print("\n--- GPU DEVICE INFO ---")
    device = cp.cuda.Device()
    props = cp.cuda.runtime.getDeviceProperties(device.id)
    print(f"Device: {props['name'].decode()}")
    print(f"Compute Capability: {props['major']}.{props['minor']}")
    print(f"Total Memory: {props['totalGlobalMem'] / (1024**3):.2f} GB")
    print(f"Multiprocessors: {props['multiProcessorCount']}")
    print(f"Clock Rate: {props['clockRate'] / 1000:.0f} MHz")


def profile_operation_comparison(rf_file, system="PAL"):
    """Compare CPU vs GPU performance for individual operations."""
    from vhsdecode.process import VHSRFDecode
    from vhsdecode.process_gpu import VHSRFDecodeGPU
    
    print(f"\nComparing CPU vs GPU operations: {rf_file}")
    print("-" * 80)
    
    # Initialize decoders
    cpu_decoder = VHSRFDecode(
        inputfreq=40,
        system=system,
        tape_format="VHS",
        rf_options={},
        extra_options={}
    )
    
    gpu_decoder = VHSRFDecodeGPU(
        inputfreq=40,
        system=system,
        tape_format="VHS",
        rf_options={},
        extra_options={'use_gpu': True, 'gpu_phase2': True}
    )
    
    # Read small sample
    with open(rf_file, 'rb') as f:
        rf_data = np.frombuffer(f.read(cpu_decoder.blocklen * 4), dtype=np.uint8).astype(np.float32)
    
    block_data = rf_data[:cpu_decoder.blocklen]
    
    # Benchmark FFT
    print("\nFFT Performance:")
    cpu_times = []
    for _ in range(10):
        start = time.perf_counter()
        fft_cpu = np.fft.fft(block_data)
        cpu_times.append(time.perf_counter() - start)
    
    gpu_times = []
    data_gpu = cp.asarray(block_data)
    for _ in range(10):
        cp.cuda.Stream.null.synchronize()
        start = time.perf_counter()
        fft_gpu = cp.fft.fft(data_gpu)
        cp.cuda.Stream.null.synchronize()
        gpu_times.append(time.perf_counter() - start)
    
    cpu_avg = np.mean(cpu_times) * 1000
    gpu_avg = np.mean(gpu_times) * 1000
    speedup = cpu_avg / gpu_avg
    
    print(f"  CPU: {cpu_avg:.3f} ms")
    print(f"  GPU: {gpu_avg:.3f} ms")
    print(f"  Speedup: {speedup:.2f}x")
    
    # Benchmark filtering
    print("\nFiltering Performance:")
    fft_data_cpu = np.fft.fft(block_data)
    filter_cpu = cpu_decoder.Filters["RFVideo"]
    
    cpu_times = []
    for _ in range(10):
        start = time.perf_counter()
        filtered = fft_data_cpu * filter_cpu
        cpu_times.append(time.perf_counter() - start)
    
    fft_data_gpu = cp.asarray(fft_data_cpu)
    filter_gpu = gpu_decoder.Filters_gpu["RFVideo"]
    
    gpu_times = []
    for _ in range(10):
        cp.cuda.Stream.null.synchronize()
        start = time.perf_counter()
        filtered_gpu = fft_data_gpu * filter_gpu
        cp.cuda.Stream.null.synchronize()
        gpu_times.append(time.perf_counter() - start)
    
    cpu_avg = np.mean(cpu_times) * 1000
    gpu_avg = np.mean(gpu_times) * 1000
    speedup = cpu_avg / gpu_avg
    
    print(f"  CPU: {cpu_avg:.3f} ms")
    print(f"  GPU: {gpu_avg:.3f} ms")
    print(f"  Speedup: {speedup:.2f}x")


def main():
    parser = argparse.ArgumentParser(
        description="Profile GPU operations in VHS-Decode",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Profile full pipeline
  python profile_gpu.py sample_data/out2.u8 --system PAL --blocks 20
  
  # Compare CPU vs GPU
  python profile_gpu.py sample_data/out2.u8 --compare
  
  # Profile with CuPy profiler
  python profile_gpu.py sample_data/out2.u8 --cupy-profile
        """
    )
    
    parser.add_argument('rf_file', help='Input RF capture file (.u8, .r8, .r16, etc.)')
    parser.add_argument('--system', default='PAL', choices=['NTSC', 'PAL', 'PAL-M', 'SECAM'],
                        help='TV system (default: PAL)')
    parser.add_argument('--blocks', type=int, default=10,
                        help='Number of blocks to profile (default: 10)')
    parser.add_argument('--compare', action='store_true',
                        help='Compare CPU vs GPU performance')
    parser.add_argument('--cupy-profile', action='store_true',
                        help='Use CuPy built-in profiler')
    
    args = parser.parse_args()
    
    # Check file exists
    if not Path(args.rf_file).exists():
        logger.error(f"File not found: {args.rf_file}")
        return 1
    
    if args.compare:
        profile_operation_comparison(args.rf_file, args.system)
    elif args.cupy_profile:
        print("\nUsing CuPy built-in profiler...")
        print("Run with: CUPTI_PROFILER_ENABLE=1 python profile_gpu.py ...")
        print("Or use: nvprof python profile_gpu.py ...")
        with cp.cuda.profile():
            profile_full_pipeline(args.rf_file, args.system, args.blocks)
    else:
        profile_full_pipeline(args.rf_file, args.system, args.blocks)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
