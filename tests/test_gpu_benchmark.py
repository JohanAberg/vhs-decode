"""
Performance benchmarks for GPU acceleration.

These tests measure and track GPU vs CPU performance to ensure
speedup targets are met.
"""

import pytest
import numpy as np
import time
from pathlib import Path

from vhsdecode.gpu_utils import GPU_AVAILABLE
from tests.benchmark_tracker import BenchmarkTracker

# Skip all GPU benchmarks if GPU is not available
pytestmark = pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")

# Import cupy conditionally
if GPU_AVAILABLE:
    import cupy as cp


def get_git_commit():
    """Get current git commit hash."""
    import subprocess
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


@pytest.fixture
def benchmark_data_32k():
    """Generate 32K sample test data."""
    np.random.seed(42)
    return np.random.randn(32768).astype(np.float32)


@pytest.fixture
def benchmark_data_64k():
    """Generate 64K sample test data."""
    np.random.seed(42)
    return np.random.randn(65536).astype(np.float32)


@pytest.fixture
def benchmark_data_128k():
    """Generate 128K sample test data."""
    np.random.seed(42)
    return np.random.randn(131072).astype(np.float32)


class TestFFTBenchmark:
    """Benchmark FFT operations."""
    
    @pytest.mark.benchmark(group="fft")
    def test_fft_32k_cpu(self, benchmark, benchmark_data_32k):
        """Benchmark CPU FFT on 32K samples."""
        result = benchmark(np.fft.fft, benchmark_data_32k)
        
    @pytest.mark.benchmark(group="fft")
    def test_fft_32k_gpu(self, benchmark, benchmark_data_32k):
        """Benchmark GPU FFT on 32K samples."""
        gpu_data = cp.asarray(benchmark_data_32k)
        
        def gpu_fft():
            result = cp.fft.fft(gpu_data)
            cp.cuda.Stream.null.synchronize()  # Ensure completion
            return result
        
        result = benchmark(gpu_fft)
        
    @pytest.mark.benchmark(group="fft")
    def test_fft_64k_cpu(self, benchmark, benchmark_data_64k):
        """Benchmark CPU FFT on 64K samples."""
        result = benchmark(np.fft.fft, benchmark_data_64k)
        
    @pytest.mark.benchmark(group="fft")
    def test_fft_64k_gpu(self, benchmark, benchmark_data_64k):
        """Benchmark GPU FFT on 64K samples."""
        gpu_data = cp.asarray(benchmark_data_64k)
        
        def gpu_fft():
            result = cp.fft.fft(gpu_data)
            cp.cuda.Stream.null.synchronize()
            return result
        
        result = benchmark(gpu_fft)


class TestFilteringBenchmark:
    """Benchmark frequency-domain filtering operations."""
    
    @pytest.mark.benchmark(group="filtering")
    def test_filter_cpu(self, benchmark, benchmark_data_32k):
        """Benchmark CPU filtering."""
        fft_data = np.fft.fft(benchmark_data_32k)
        filter_data = np.random.randn(len(benchmark_data_32k)).astype(np.float32)
        
        def cpu_filter():
            filtered = fft_data * filter_data
            result = np.fft.ifft(filtered).real
            return result
        
        result = benchmark(cpu_filter)
        
    @pytest.mark.benchmark(group="filtering")
    def test_filter_gpu(self, benchmark, benchmark_data_32k):
        """Benchmark GPU filtering."""
        gpu_data = cp.asarray(benchmark_data_32k)
        fft_data = cp.fft.fft(gpu_data)
        filter_data = cp.random.randn(len(benchmark_data_32k)).astype(cp.float32)
        
        def gpu_filter():
            filtered = fft_data * filter_data
            result = cp.fft.ifft(filtered).real
            cp.cuda.Stream.null.synchronize()
            return result
        
        result = benchmark(gpu_filter)


class TestHilbertBenchmark:
    """Benchmark Hilbert transform operations."""
    
    @pytest.mark.benchmark(group="hilbert")
    def test_hilbert_cpu(self, benchmark, benchmark_data_32k):
        """Benchmark CPU Hilbert transform."""
        from scipy import signal
        
        result = benchmark(signal.hilbert, benchmark_data_32k)
        
    @pytest.mark.benchmark(group="hilbert")
    def test_hilbert_gpu(self, benchmark, benchmark_data_32k):
        """Benchmark GPU Hilbert transform (via FFT)."""
        gpu_data = cp.asarray(benchmark_data_32k)
        
        def gpu_hilbert(data):
            """Simple Hilbert transform implementation on GPU."""
            N = len(data)
            fft_data = cp.fft.fft(data)
            
            # Create Hilbert multiplier
            h = cp.zeros(N)
            if N % 2 == 0:
                h[0] = h[N // 2] = 1
                h[1:N // 2] = 2
            else:
                h[0] = 1
                h[1:(N + 1) // 2] = 2
            
            result = cp.fft.ifft(fft_data * h)
            cp.cuda.Stream.null.synchronize()
            return result
        
        result = benchmark(gpu_hilbert, gpu_data)


class TestComplexOperationsBenchmark:
    """Benchmark complex number operations."""
    
    @pytest.mark.benchmark(group="complex")
    def test_angle_cpu(self, benchmark):
        """Benchmark CPU angle calculation."""
        np.random.seed(42)
        complex_data = np.random.randn(32768).astype(np.complex64)
        
        result = benchmark(np.angle, complex_data)
        
    @pytest.mark.benchmark(group="complex")
    def test_angle_gpu(self, benchmark):
        """Benchmark GPU angle calculation."""
        np.random.seed(42)
        complex_data = np.random.randn(32768).astype(np.complex64)
        gpu_data = cp.asarray(complex_data)
        
        def gpu_angle():
            result = cp.angle(gpu_data)
            cp.cuda.Stream.null.synchronize()
            return result
        
        result = benchmark(gpu_angle)


class TestEndToEndBenchmark:
    """End-to-end benchmarks simulating full decode pipeline."""
    
    @pytest.mark.benchmark(group="pipeline")
    def test_demod_simulation_cpu(self, benchmark, benchmark_data_32k):
        """Benchmark simulated demodulation on CPU."""
        def cpu_demod_sim(data):
            # Simulate demodulation pipeline:
            # 1. FFT
            fft_data = np.fft.fft(data)
            
            # 2. Apply filter
            filter_kernel = np.random.randn(len(data)).astype(np.float32)
            filtered = fft_data * filter_kernel
            
            # 3. Inverse FFT
            demod = np.fft.ifft(filtered)
            
            # 4. Angle calculation
            angle = np.angle(demod)
            
            # 5. Another FFT for post-processing
            post_fft = np.fft.fft(angle)
            
            # 6. Apply another filter
            post_filtered = post_fft * filter_kernel
            
            # 7. Final inverse FFT
            result = np.fft.ifft(post_filtered).real
            
            return result
        
        result = benchmark(cpu_demod_sim, benchmark_data_32k)
        
    @pytest.mark.benchmark(group="pipeline")
    def test_demod_simulation_gpu(self, benchmark, benchmark_data_32k):
        """Benchmark simulated demodulation on GPU."""
        gpu_data = cp.asarray(benchmark_data_32k)
        filter_kernel = cp.random.randn(len(benchmark_data_32k)).astype(cp.float32)
        
        def gpu_demod_sim(data):
            # Simulate demodulation pipeline on GPU:
            # 1. FFT
            fft_data = cp.fft.fft(data)
            
            # 2. Apply filter
            filtered = fft_data * filter_kernel
            
            # 3. Inverse FFT
            demod = cp.fft.ifft(filtered)
            
            # 4. Angle calculation
            angle = cp.angle(demod)
            
            # 5. Another FFT for post-processing
            post_fft = cp.fft.fft(angle)
            
            # 6. Apply another filter
            post_filtered = post_fft * filter_kernel
            
            # 7. Final inverse FFT
            result = cp.fft.ifft(post_filtered).real
            
            # Synchronize to ensure completion
            cp.cuda.Stream.null.synchronize()
            
            return result
        
        result = benchmark(gpu_demod_sim, gpu_data)


class TestTransferOverhead:
    """Test CPU<->GPU transfer overhead."""
    
    @pytest.mark.benchmark(group="transfer")
    def test_cpu_to_gpu_32k(self, benchmark, benchmark_data_32k):
        """Benchmark CPU to GPU transfer for 32K samples."""
        def transfer():
            gpu_data = cp.asarray(benchmark_data_32k)
            cp.cuda.Stream.null.synchronize()
            return gpu_data
        
        result = benchmark(transfer)
        
    @pytest.mark.benchmark(group="transfer")
    def test_gpu_to_cpu_32k(self, benchmark, benchmark_data_32k):
        """Benchmark GPU to CPU transfer for 32K samples."""
        gpu_data = cp.asarray(benchmark_data_32k)
        
        def transfer():
            cpu_data = cp.asnumpy(gpu_data)
            return cpu_data
        
        result = benchmark(transfer)
        
    @pytest.mark.benchmark(group="transfer")
    def test_roundtrip_32k(self, benchmark, benchmark_data_32k):
        """Benchmark CPU->GPU->CPU roundtrip for 32K samples."""
        def roundtrip():
            gpu_data = cp.asarray(benchmark_data_32k)
            cpu_data = cp.asnumpy(gpu_data)
            return cpu_data
        
        result = benchmark(roundtrip)


def test_speedup_tracking(benchmark_data_32k):
    """
    Track speedup metrics and ensure targets are met.
    
    This test doesn't use pytest-benchmark but instead manually measures
    and tracks speedup to integrate with our benchmark tracker.
    """
    # Prepare data
    gpu_data = cp.asarray(benchmark_data_32k)
    filter_kernel = np.random.randn(len(benchmark_data_32k)).astype(np.float32)
    gpu_filter_kernel = cp.asarray(filter_kernel)
    
    # Measure CPU time
    iterations = 10
    cpu_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        fft_data = np.fft.fft(benchmark_data_32k)
        filtered = fft_data * filter_kernel
        result = np.fft.ifft(filtered).real
        cpu_times.append(time.perf_counter() - start)
    cpu_time = np.median(cpu_times)
    
    # Measure GPU time
    gpu_times = []
    for _ in range(iterations):
        cp.cuda.Stream.null.synchronize()  # Ensure GPU is ready
        start = time.perf_counter()
        fft_data = cp.fft.fft(gpu_data)
        filtered = fft_data * gpu_filter_kernel
        result = cp.fft.ifft(filtered).real
        cp.cuda.Stream.null.synchronize()  # Ensure completion
        gpu_times.append(time.perf_counter() - start)
    gpu_time = np.median(gpu_times)
    
    # Calculate speedup
    speedup = cpu_time / gpu_time
    
    # Track results
    tracker = BenchmarkTracker("tests/benchmark_results.json")
    tracker.record(
        test_name="fft_filter_pipeline_32k",
        cpu_time=cpu_time,
        gpu_time=gpu_time,
        commit_hash=get_git_commit()
    )
    
    # Assert minimum speedup (Phase 1 target: 2x)
    assert speedup >= 1.5, f"GPU speedup {speedup:.2f}x is below minimum 1.5x"
    
    print(f"\nSpeedup: {speedup:.2f}x (CPU: {cpu_time*1000:.2f}ms, GPU: {gpu_time*1000:.2f}ms)")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--benchmark-only"])
