"""
GPU utility functions for VHS-Decode.

This module handles GPU detection, array module selection, and GPU-related utilities.
"""

import logging
import sys
import time
from collections import defaultdict
from threading import Lock

logger = logging.getLogger(__name__)

# Try to import CuPy for GPU acceleration
try:
    import cupy as cp
    GPU_AVAILABLE = True
    logger.info("GPU acceleration available (CuPy found)")
except ImportError:
    cp = None
    GPU_AVAILABLE = False
    logger.info("GPU acceleration not available (CuPy not installed)")


def get_array_module(use_gpu=True):
    """
    Return cupy if available and requested, else numpy.
    
    This allows for writing code that works with both CPU and GPU arrays
    using the same interface.
    
    Args:
        use_gpu (bool): Whether to use GPU if available
        
    Returns:
        module: cupy if GPU is available and requested, else numpy
        
    Example:
        >>> xp = get_array_module(use_gpu=True)
        >>> data = xp.array([1, 2, 3])  # Creates GPU or CPU array
    """
    import numpy as np
    
    if use_gpu and GPU_AVAILABLE:
        return cp
    return np


def get_gpu_info():
    """
    Get information about available GPU(s).
    
    Returns:
        dict: Dictionary with GPU information or None if no GPU available
        
    Example:
        >>> info = get_gpu_info()
        >>> if info:
        ...     print(f"GPU: {info['name']}, VRAM: {info['memory_total']/1e9:.1f} GB")
    """
    if not GPU_AVAILABLE:
        return None
    
    try:
        device = cp.cuda.Device()
        
        # Safely get device name using proper CuPy API
        try:
            name = cp.cuda.runtime.getDeviceProperties(device.id)['name']
            if isinstance(name, bytes):
                name = name.decode('utf-8', errors='ignore')
        except:
            # Fallback if properties not available
            name = f'GPU Device {device.id}'
        
        # Safely get PCI bus ID
        pci_bus_id = 'unknown'
        if hasattr(device, 'pci_bus_id'):
            bus_id = device.pci_bus_id
            if bus_id is not None:
                pci_bus_id = bus_id.decode('utf-8', errors='ignore') if isinstance(bus_id, bytes) else str(bus_id)
        
        return {
            'name': name,
            'compute_capability': device.compute_capability,
            'memory_total': device.mem_info[1],
            'memory_free': device.mem_info[0],
            'pci_bus_id': pci_bus_id
        }
    except Exception as e:
        logger.warning(f"Failed to get GPU info: {e}")
        return None


def check_gpu_memory(required_bytes):
    """
    Check if enough GPU memory is available.
    
    Args:
        required_bytes (int): Required memory in bytes
        
    Returns:
        bool: True if enough memory is available
    """
    if not GPU_AVAILABLE:
        return False
    
    try:
        device = cp.cuda.Device()
        free_mem = device.mem_info[0]
        return free_mem >= required_bytes
    except Exception:
        return False


def free_gpu_memory():
    """
    Free all blocks in the default GPU memory pool.
    
    Call this periodically to avoid memory fragmentation.
    """
    if GPU_AVAILABLE:
        try:
            cp.get_default_memory_pool().free_all_blocks()
        except Exception as e:
            logger.warning(f"Failed to free GPU memory: {e}")


def transfer_to_gpu(data, stream=None):
    """
    Transfer numpy array to GPU if available.
    Uses optimized asynchronous transfer for better performance.
    
    Args:
        data: numpy array or cupy array
        stream: Optional CUDA stream for async transfer
        
    Returns:
        cupy array if GPU available, else original numpy array
    """
    if not GPU_AVAILABLE:
        return data
    
    # If already on GPU, return as-is
    if isinstance(data, cp.ndarray):
        return data
    
    # Transfer to GPU with optimized path
    try:
        # Use asarray which is optimized for contiguous arrays
        # CuPy automatically uses pinned memory when beneficial
        if stream is not None:
            # Async transfer if stream provided
            gpu_array = cp.empty(data.shape, dtype=data.dtype)
            gpu_array.set(data, stream=stream)
            return gpu_array
        else:
            return cp.asarray(data)
    except Exception as e:
        logger.warning(f"Failed to transfer data to GPU: {e}")
        return data


def transfer_from_gpu(data, stream=None):
    """
    Transfer array from GPU to CPU.
    Uses optimized asynchronous transfer for better performance.
    
    Args:
        data: cupy array or numpy array
        stream: Optional CUDA stream for async transfer
        
    Returns:
        numpy array
    """
    if not GPU_AVAILABLE:
        return data
    
    # If already on CPU, return as-is
    if not isinstance(data, cp.ndarray):
        return data
    
    # Transfer from GPU with optimized path
    try:
        if stream is not None:
            # Async transfer if stream provided
            cpu_array = cp.asnumpy(data, stream=stream)
            return cpu_array
        else:
            return cp.asnumpy(data)
    except Exception as e:
        logger.warning(f"Failed to transfer data from GPU: {e}")
        return data


class GPUError(Exception):
    """Base exception for GPU-related errors."""
    pass


class GPUOutOfMemoryError(GPUError):
    """Raised when GPU runs out of memory."""
    pass


class StreamManager:
    """
    Manages CUDA streams for overlapping data transfers and computation.
    
    Usage:
        with StreamManager() as streams:
            # Transfer on stream 0
            data_gpu = transfer_to_gpu(data, stream=streams.transfer)
            # Compute on stream 1
            with streams.compute:
                result = process_on_gpu(data_gpu)
    """
    def __init__(self):
        self.transfer = None
        self.compute = None
        
    def __enter__(self):
        if GPU_AVAILABLE:
            self.transfer = cp.cuda.Stream()
            self.compute = cp.cuda.Stream()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if GPU_AVAILABLE:
            if self.transfer:
                self.transfer.synchronize()
            if self.compute:
                self.compute.synchronize()
        return False
    
    def synchronize(self):
        """Wait for all streams to complete."""
        if GPU_AVAILABLE:
            if self.transfer:
                self.transfer.synchronize()
            if self.compute:
                self.compute.synchronize()


def log_gpu_status():
    """Log current GPU status and availability."""
    if GPU_AVAILABLE:
        info = get_gpu_info()
        if info:
            logger.info(f"GPU: {info['name']}")
            logger.info(f"Compute Capability: {info['compute_capability']}")
            logger.info(f"Total VRAM: {info['memory_total']/1e9:.2f} GB")
            logger.info(f"Free VRAM: {info['memory_free']/1e9:.2f} GB")
        else:
            logger.info("GPU available but unable to get device info")
    else:
        logger.info("No GPU available - using CPU only")


def demod_chroma_filt_gpu(data_gpu, filter_gpu, blocklen, notch_gpu=None, do_notch=False, move=10):
    """
    GPU-accelerated chroma demodulation and filtering.
    
    This function performs chroma processing entirely on GPU using FFT-based filtering,
    avoiding CPU fallback and GPU↔CPU transfers. Replaces the CPU-only demod_chroma_filt().
    
    Args:
        data_gpu: Input data (CuPy array on GPU)
        filter_gpu: Burst filter in frequency domain (CuPy array)
        blocklen: Block length for processing
        notch_gpu: Optional notch filter in frequency domain (CuPy array)
        do_notch: Whether to apply notch filter
        move: Chroma offset compensation (default: 10)
    
    Returns:
        CuPy array: Filtered chroma data on GPU
    
    Performance: Optimized to combine filters and operations in frequency domain.
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for demod_chroma_filt_gpu")
    
    # Take only blocklen samples
    if data_gpu.ndim == 1:
        data_block = data_gpu[:blocklen]
    else:
        data_block = data_gpu[..., :blocklen]
    
    # FFT
    data_fft = cp.fft.rfft(data_block)
    
    # Apply burst filter
    # If notch is enabled, we can combine it in frequency domain to save an IFFT/FFT pair
    if do_notch and notch_gpu is not None:
        # Combine filters (element-wise multiplication)
        # Note: We modify data_fft in place to save memory
        data_fft *= filter_gpu
        data_fft *= notch_gpu
    else:
        data_fft *= filter_gpu
        
    # Remove DC offset in frequency domain (much faster than mean() reduction)
    # DC component is at index 0
    if data_fft.ndim == 1:
        data_fft[0] = 0.0
    else:
        data_fft[..., 0] = 0.0
    
    # IFFT
    out_chroma = cp.fft.irfft(data_fft).real
    
    # Roll to compensate for Y filter delay
    # We could do this in freq domain with phase shift, but cp.roll is fast enough
    if move != 0:
        out_chroma = cp.roll(out_chroma, move)
    
    # Convert to float32 to match CPU version output type
    out_chroma = out_chroma.astype(cp.float32)
    
    return out_chroma


class GPUProfiler:
    """Lightweight GPU profiler for performance analysis (thread-safe)."""
    
    def __init__(self):
        self.timings = defaultdict(list)
        self.active_timer = None
        self.timer_start = None
        self.transfer_bytes = {'cpu_to_gpu': 0, 'gpu_to_cpu': 0}
        self.peak_memory = 0
        self.lock = Lock()  # Thread safety for multi-threaded profiling
        
    def start(self, name):
        """Start timing an operation."""
        if not GPU_AVAILABLE:
            return
        
        self.active_timer = name
        if GPU_AVAILABLE:
            cp.cuda.Stream.null.synchronize()
        self.timer_start = time.perf_counter()
        
    def stop(self, name=None):
        """Stop timing and record the elapsed time."""
        if not GPU_AVAILABLE:
            return 0
        
        if GPU_AVAILABLE:
            cp.cuda.Stream.null.synchronize()
        elapsed = time.perf_counter() - self.timer_start
        
        with self.lock:
            if name is None:
                name = self.active_timer
            
            # Skip if name is still None (no active timer)
            if name is not None:
                self.timings[name].append(elapsed)
            self.active_timer = None
        return elapsed
        
    def record_transfer(self, direction, nbytes):
        """Record memory transfer size."""
        with self.lock:
            if direction == 'cpu_to_gpu':
                self.transfer_bytes['cpu_to_gpu'] += nbytes
            elif direction == 'gpu_to_cpu':
                self.transfer_bytes['gpu_to_cpu'] += nbytes
            
    def update_memory(self):
        """Update peak memory usage."""
        if not GPU_AVAILABLE:
            return
        
        mempool = cp.get_default_memory_pool()
        used = mempool.used_bytes()
        with self.lock:
            self.peak_memory = max(self.peak_memory, used)
        
    def print_summary(self):
        """Print profiling summary."""
        if not self.timings:
            print("\nNo profiling data collected")
            return
        
        print("\n" + "="*70)
        print("GPU PROFILING SUMMARY")
        print("="*70)
        
        # Filter out empty timing lists
        valid_timings = {k: v for k, v in self.timings.items() if v}
        if not valid_timings:
            print("\nNo valid profiling data collected")
            return
        
        total_time = sum(sum(times) for times in valid_timings.values())
        if total_time == 0:
            print("\nNo profiling time recorded")
            return
        
        print(f"\n{'Operation':<35} {'Count':>8} {'Total(ms)':>12} {'Avg(ms)':>10} {'%':>6}")
        print("-" * 70)
        
        sorted_timings = sorted(
            valid_timings.items(),
            key=lambda x: sum(x[1]) if x[1] else 0,
            reverse=True
        )
        
        for name, times in sorted_timings:
            if not times or name is None:
                continue
            count = len(times)
            total_ms = sum(times) * 1000
            avg_ms = (sum(times) / count) * 1000 if count > 0 else 0
            pct = (sum(times) / total_time) * 100 if total_time > 0 else 0
            # Ensure name is string
            name_str = str(name) if name is not None else "unknown"
            print(f"{name_str:<35} {count:>8} {total_ms:>12.2f} {avg_ms:>10.3f} {pct:>5.1f}%")
        
        print("-" * 70)
        print(f"{'TOTAL':<35} {'':<8} {total_time*1000:>12.2f} {'':<10} {'100.0':>5}%")
        
        # Memory transfers
        cpu_to_gpu_mb = self.transfer_bytes['cpu_to_gpu'] / (1024**2)
        gpu_to_cpu_mb = self.transfer_bytes['gpu_to_cpu'] / (1024**2)
        
        print(f"\nMemory Transfers:")
        print(f"  CPU->GPU: {cpu_to_gpu_mb:.2f} MB")
        print(f"  GPU->CPU: {gpu_to_cpu_mb:.2f} MB")
        print(f"  Peak GPU Memory: {self.peak_memory / (1024**2):.2f} MB")
        print("="*70 + "\n")


# Global profiler instance
_gpu_profiler = None

def get_profiler():
    """Get or create the global GPU profiler."""
    global _gpu_profiler
    if _gpu_profiler is None:
        _gpu_profiler = GPUProfiler()
    return _gpu_profiler

def get_active_profiler():
    """Get the global GPU profiler if it exists, else None."""
    global _gpu_profiler
    return _gpu_profiler

def enable_profiling():
    """Enable GPU profiling for all operations."""
    global _gpu_profiler
    _gpu_profiler = GPUProfiler()
    return _gpu_profiler


def setup_gpu_memory_pools(blocklen, batch_size=10, target_vram_usage_gb=2.0):
    """
    Pre-allocate CuPy memory pools for common operations to reduce allocation overhead.
    
    This function calculates the expected memory requirements based on block size
    and batch count, then pre-allocates GPU memory pools. This dramatically reduces
    runtime memory allocation overhead.
    
    Args:
        blocklen: Length of RF data blocks (e.g., 32768 or 131072)
        batch_size: Number of blocks to process simultaneously (default: 10)
        target_vram_usage_gb: Target VRAM to pre-allocate in GB (default: 2.0)
        
    Example:
        >>> setup_gpu_memory_pools(blocklen=131072, batch_size=10)
        >>> # Subsequent allocations will be much faster
        
    Performance Impact:
        - Eliminates 1-2 seconds of allocation overhead per decode
        - Reduces memory fragmentation
        - Enables larger batch processing
    """
    if not GPU_AVAILABLE:
        logger.warning("Cannot setup GPU memory pools: GPU not available")
        return
    
    try:
        # Calculate typical memory needs per block
        # Based on GPU_BOTTLENECK_ANALYSIS: typical block uses ~1.5-2.5 MB
        
        # RF input data (float64)
        rf_input_bytes = blocklen * 8
        
        # FFT result (complex128)
        fft_result_bytes = blocklen * 16
        
        # Filtered data (float64)
        filtered_bytes = blocklen * 8
        
        # Output data (float64)
        output_bytes = blocklen * 8
        
        # Intermediate buffers for envelope, chroma, etc.
        intermediate_bytes = blocklen * 8 * 3  # Estimate for 3 intermediate buffers
        
        # Total per block
        total_per_block = (rf_input_bytes + fft_result_bytes + filtered_bytes + 
                          output_bytes + intermediate_bytes)
        
        # Total for batch
        total_for_batch = total_per_block * batch_size
        
        # Convert target VRAM to bytes
        target_bytes = int(target_vram_usage_gb * 1e9)
        
        # Use the larger of batch size or target VRAM
        pool_size = max(total_for_batch, target_bytes)
        
        logger.info(f"Pre-allocating GPU memory pool:")
        logger.info(f"  Block size: {blocklen} samples")
        logger.info(f"  Memory per block: {total_per_block / 1e6:.2f} MB")
        logger.info(f"  Batch size: {batch_size} blocks")
        logger.info(f"  Batch memory: {total_for_batch / 1e6:.2f} MB")
        logger.info(f"  Pool size: {pool_size / 1e6:.2f} MB")
        
        # Get current GPU memory info
        device = cp.cuda.Device()
        mem_total = device.mem_info[1]
        mem_free = device.mem_info[0]
        
        # Check if we have enough free memory
        if pool_size > mem_free:
            logger.warning(f"Requested pool size ({pool_size/1e9:.2f} GB) exceeds free VRAM "
                         f"({mem_free/1e9:.2f} GB). Adjusting to 80% of free VRAM.")
            pool_size = int(mem_free * 0.8)
        
        # Get current pool state before allocation
        mempool = cp.get_default_memory_pool()
        current_limit = mempool.get_limit()
        
        # Calculate final pool limit upfront
        # Allow 2x the pool size for safety margin
        pool_limit = min(pool_size * 2, int(mem_total * 0.9))  # Max 90% of total VRAM
        
        # If current limit is too low, increase it BEFORE allocation attempt
        # Must set limit before any allocation that exceeds current limit
        if current_limit > 0 and pool_size > current_limit:
            logger.info(f"Current pool limit ({current_limit/1e9:.2f} GB) too low for {pool_size/1e9:.2f} GB allocation")
            logger.info(f"Increasing pool limit to {pool_limit/1e9:.2f} GB...")
            mempool.set_limit(size=pool_limit)
        elif current_limit == 0:
            # No limit set yet, set it now
            mempool.set_limit(size=pool_limit)
        
        # Allocate and free to setup the pool (this initializes the memory pool)
        logger.info(f"Initializing memory pool with {pool_size/1e6:.2f} MB...")
        dummy = cp.empty(pool_size, dtype=cp.uint8)
        del dummy
        
        logger.info(f"GPU memory pool configured:")
        logger.info(f"  Initial pool: {pool_size/1e6:.2f} MB")
        logger.info(f"  Pool limit: {pool_limit/1e6:.2f} MB ({pool_limit/mem_total*100:.1f}% of total VRAM)")
        logger.info(f"  Expected VRAM usage: {pool_size/mem_total*100:.1f}% of {mem_total/1e9:.2f} GB")
        
    except Exception as e:
        logger.error(f"Failed to setup GPU memory pools: {e}")
        # Non-fatal - continue with default memory management
