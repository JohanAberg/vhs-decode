"""
Unit tests for GPU-accelerated functions.

These tests verify that GPU implementations produce identical results
to CPU implementations.
"""

import pytest
import numpy as np
from pathlib import Path

# Import GPU utilities
from vhsdecode.gpu_utils import GPU_AVAILABLE, get_array_module, get_gpu_info, free_gpu_memory
from vhsdecode.cuda_kernels.fused_fm_demod import unwrap_hilbert_gpu_fused, is_fused_kernel_available

# Conditional import of cupy with CUDA error handling
if GPU_AVAILABLE:
    try:
        import cupy as cp
        # Test basic CUDA functionality including FFT libraries
        test_array = cp.array([1, 2, 3, 4])
        # Try basic operations that require CUDA runtime
        cp.fft.fft(test_array.astype(cp.complex64))
        cp.random.randn(10)
        test_mult = test_array * 2.0  # Test kernel compilation
        CUDA_FUNCTIONAL = True
    except Exception as e:
        # CUDA libraries not available, mark for skipping computation-heavy tests
        CUDA_FUNCTIONAL = False
        cp = None
        print(f"Warning: GPU detected but CUDA not functional: {e}")
else:
    CUDA_FUNCTIONAL = False
    cp = None

if CUDA_FUNCTIONAL:
    try:
        FUSED_KERNEL_AVAILABLE = bool(is_fused_kernel_available())
    except Exception as e:
        FUSED_KERNEL_AVAILABLE = False
        print(f"Warning: Fused FM kernel not available: {e}")
else:
    FUSED_KERNEL_AVAILABLE = False


# Skip all GPU tests if GPU is not available
pytestmark = pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")


class TestGPUUtilities:
    """Test basic GPU utility functions."""
    
    def test_gpu_available(self):
        """Test that GPU is properly detected."""
        assert GPU_AVAILABLE is True
        
    def test_get_array_module_gpu(self):
        """Test getting array module with GPU enabled."""
        xp = get_array_module(use_gpu=True)
        assert xp.__name__ == 'cupy'
        
    def test_get_array_module_cpu(self):
        """Test getting array module with GPU disabled."""
        xp = get_array_module(use_gpu=False)
        assert xp.__name__ == 'numpy'
        
    def test_get_gpu_info(self):
        """Test GPU info retrieval."""
        info = get_gpu_info()
        assert info is not None
        assert 'name' in info
        assert 'memory_total' in info
        assert 'memory_free' in info
        assert info['memory_total'] > 0
        
    def test_free_gpu_memory(self):
        """Test GPU memory cleanup."""
        # Should not raise any exceptions
        free_gpu_memory()


class TestGPUArrayOperations:
    """Test basic GPU array operations."""
    
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_array_creation(self):
        """Test creating arrays on GPU."""
        xp = get_array_module(use_gpu=True)
        data = xp.array([1, 2, 3, 4, 5])
        assert isinstance(data, cp.ndarray)
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_array_transfer_to_gpu(self):
        """Test transferring numpy array to GPU."""
        from vhsdecode.gpu_utils import transfer_to_gpu
        cpu_data = np.array([1, 2, 3, 4, 5])
        gpu_data = transfer_to_gpu(cpu_data)
        assert isinstance(gpu_data, cp.ndarray)
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_array_transfer_from_gpu(self):
        """Test transferring GPU array to CPU."""
        from vhsdecode.gpu_utils import transfer_from_gpu
        gpu_data = cp.array([1, 2, 3, 4, 5])
        cpu_data = transfer_from_gpu(gpu_data)
        assert isinstance(cpu_data, np.ndarray)
        np.testing.assert_array_equal(cpu_data, [1, 2, 3, 4, 5])


class TestGPUFFT:
    """Test GPU FFT operations match CPU FFT."""
    
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_fft_equivalence(self):
        """Verify GPU FFT matches CPU FFT."""
        # Create test data
        np.random.seed(42)
        test_data = np.random.randn(32768).astype(np.float32)
        
        # CPU FFT
        cpu_result = np.fft.fft(test_data)
        
        # GPU FFT
        gpu_data = cp.asarray(test_data)
        gpu_result = cp.fft.fft(gpu_data)
        gpu_result_cpu = cp.asnumpy(gpu_result)
        
        # Compare (GPU FFT has slightly different precision than CPU)
        np.testing.assert_allclose(cpu_result, gpu_result_cpu, rtol=1e-4, atol=1e-4)
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_ifft_equivalence(self):
        """Verify GPU iFFT matches CPU iFFT."""
        # Create test data in frequency domain
        np.random.seed(42)
        test_data = np.random.randn(32768).astype(np.complex64)
        
        # CPU iFFT
        cpu_result = np.fft.ifft(test_data)
        
        # GPU iFFT
        gpu_data = cp.asarray(test_data)
        gpu_result = cp.fft.ifft(gpu_data)
        gpu_result_cpu = cp.asnumpy(gpu_result)
        
        # Compare
        np.testing.assert_allclose(cpu_result, gpu_result_cpu, rtol=1e-6, atol=1e-6)
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_rfft_equivalence(self):
        """Verify GPU rfft matches CPU rfft."""
        # Create test data (real)
        np.random.seed(42)
        test_data = np.random.randn(32768).astype(np.float32)
        
        # CPU rfft
        cpu_result = np.fft.rfft(test_data)
        
        # GPU rfft
        gpu_data = cp.asarray(test_data)
        gpu_result = cp.fft.rfft(gpu_data)
        gpu_result_cpu = cp.asnumpy(gpu_result)
        
        # Compare (GPU RFFT has slightly different precision than CPU)
        np.testing.assert_allclose(cpu_result, gpu_result_cpu, rtol=1e-4, atol=1e-4)


class TestGPUFiltering:
    """Test GPU frequency-domain filtering."""
    
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_filter_multiplication(self):
        """Test frequency-domain filter application on GPU."""
        np.random.seed(42)
        
        # Create test FFT data and filter
        test_fft = np.random.randn(32768).astype(np.complex64)
        test_filter = np.random.randn(32768).astype(np.float32)
        
        # CPU filtering
        cpu_filtered = test_fft * test_filter
        
        # GPU filtering
        gpu_fft = cp.asarray(test_fft)
        gpu_filter = cp.asarray(test_filter)
        gpu_filtered = gpu_fft * gpu_filter
        gpu_filtered_cpu = cp.asnumpy(gpu_filtered)
        
        # Compare
        np.testing.assert_allclose(cpu_filtered, gpu_filtered_cpu, rtol=1e-6, atol=1e-6)
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_complex_operations(self):
        """Test complex number operations on GPU."""
        np.random.seed(42)
        test_data = np.random.randn(1000).astype(np.complex64)
        
        # CPU operations
        cpu_angle = np.angle(test_data)
        cpu_abs = np.abs(test_data)
        
        # GPU operations
        gpu_data = cp.asarray(test_data)
        gpu_angle = cp.angle(gpu_data)
        gpu_abs = cp.abs(gpu_data)
        
        # Compare
        np.testing.assert_allclose(cpu_angle, cp.asnumpy(gpu_angle), rtol=1e-6, atol=1e-6)
        np.testing.assert_allclose(cpu_abs, cp.asnumpy(gpu_abs), rtol=1e-6, atol=1e-6)


class TestGPUMemoryManagement:
    """Test GPU memory management."""
    
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_no_memory_leak(self):
        """Verify no memory leaks during repeated GPU operations."""
        xp = get_array_module(use_gpu=True)
        
        # Record initial memory
        device = cp.cuda.Device()
        initial_mem = device.mem_info[0]
        
        # Process multiple blocks
        for i in range(100):
            data = xp.random.randn(32768).astype(xp.float32)
            fft_result = xp.fft.fft(data)
            filtered = fft_result * 0.5
            result = xp.fft.ifft(filtered)
            # Explicitly delete to ensure cleanup
            del data, fft_result, filtered, result
        
        # Force garbage collection
        free_gpu_memory()
        
        # Check memory after processing
        final_mem = device.mem_info[0]
        mem_growth = initial_mem - final_mem
        
        # Allow up to 100MB growth for internal buffers/caching
        assert mem_growth < 100 * 1024 * 1024, \
            f"Memory leak detected: {mem_growth/1e6:.1f}MB growth"
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_memory_pool_cleanup(self):
        """Test memory pool cleanup."""
        # Allocate some memory
        data = cp.random.randn(1000000)
        del data
        
        # Memory pool should still have allocated blocks
        pool = cp.get_default_memory_pool()
        used_before = pool.used_bytes()
        
        # Free all blocks
        free_gpu_memory()
        
        # Memory usage should decrease
        used_after = pool.used_bytes()
        assert used_after <= used_before


class TestGPUPrecision:
    """Test numerical precision of GPU operations."""
    
    def test_fp32_vs_fp64_fft(self):
        """Compare FP32 and FP64 FFT precision."""
        np.random.seed(42)
        test_data = np.random.randn(32768)
        
        # FP64 FFT (reference)
        fp64_data = test_data.astype(np.float64)
        fp64_result = np.fft.fft(fp64_data)
        
        # FP32 FFT
        fp32_data = test_data.astype(np.float32)
        fp32_result = np.fft.fft(fp32_data)
        
        # Compare - FP32 should be close but not identical
        # Allow more tolerance for FP32
        np.testing.assert_allclose(
            fp64_result.astype(np.complex64),
            fp32_result.astype(np.complex64),
            rtol=1e-5,  # Relaxed tolerance for FP32
            atol=1e-5
        )
        
    @pytest.mark.skipif(not CUDA_FUNCTIONAL, reason="CUDA libraries not available")
    def test_gpu_cpu_precision_match(self):
        """Verify GPU and CPU produce similar precision."""
        np.random.seed(42)
        test_data = np.random.randn(10000).astype(np.float32)
        
        # CPU operation
        cpu_result = np.fft.fft(test_data)
        cpu_power = np.abs(cpu_result) ** 2
        
        # GPU operation
        gpu_data = cp.asarray(test_data)
        gpu_result = cp.fft.fft(gpu_data)
        gpu_power = cp.abs(gpu_result) ** 2
        gpu_power_cpu = cp.asnumpy(gpu_power)
        
        # Compare power spectrum (more forgiving than complex values)
        np.testing.assert_allclose(cpu_power, gpu_power_cpu, rtol=1e-4, atol=1e-4)


class TestFusedFMDemod:
    """Test fused FM demodulation kernel output."""

    @pytest.mark.skipif(not FUSED_KERNEL_AVAILABLE, reason="Fused FM kernel not available")
    def test_fused_fm_matches_reference(self):
        """Fused kernel should match reference CuPy ops, including angle wrap."""
        freq_hz = 40_000_000.0
        angles = cp.asarray([
            0.0,
            0.5 * cp.pi,
            cp.pi,
            -0.75 * cp.pi,
            -0.5 * cp.pi,
            -0.25 * cp.pi,
        ], dtype=cp.float64)
        hilbert = cp.exp(1j * angles)

        fused = unwrap_hilbert_gpu_fused(hilbert, freq_hz)

        phase_diff = cp.angle(hilbert[1:] * cp.conj(hilbert[:-1]))
        phase_diff = cp.where(phase_diff < 0, phase_diff + 2.0 * cp.pi, phase_diff)
        expected = cp.concatenate([cp.zeros(1, dtype=cp.float64), phase_diff]) * (freq_hz / (2.0 * cp.pi))

        np.testing.assert_allclose(cp.asnumpy(fused), cp.asnumpy(expected), rtol=1e-12, atol=1e-12)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
