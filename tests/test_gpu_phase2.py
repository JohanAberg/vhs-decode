"""
Unit tests for Phase 2 GPU optimizations.

Tests the new GPU-accelerated operations that reduce CPU↔GPU transfers.
"""

import pytest
import numpy as np

# Try to import GPU modules
try:
    import cupy as cp
    from vhsdecode.gpu_demod import (
        unwrap_hilbert_gpu,
        replace_spikes_gpu,
        envelope_filter_gpu,
        nonlinear_deemphasis_gpu,
    )
    GPU_AVAILABLE = True
except ImportError:
    GPU_AVAILABLE = False
    pytestmark = pytest.mark.skip("GPU not available (CuPy not installed)")


@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestUnwrapHilbertGPU:
    """Test GPU-accelerated Hilbert FM demodulation."""
    
    def test_unwrap_hilbert_basic(self):
        """Test basic unwrap_hilbert_gpu functionality."""
        # Create test signal - simple complex exponential
        freq_hz = 40e6
        n_samples = 1024
        phase = np.linspace(0, 4 * np.pi, n_samples)
        hilbert_cpu = np.exp(1j * phase).astype(np.complex128)
        
        # Transfer to GPU
        hilbert_gpu = cp.asarray(hilbert_cpu)
        
        # Process on GPU
        result_gpu = unwrap_hilbert_gpu(hilbert_gpu, freq_hz)
        result_cpu = cp.asnumpy(result_gpu)
        
        # Check output shape
        assert result_cpu.shape == hilbert_cpu.shape
        
        # Check output type
        assert result_cpu.dtype in [np.float32, np.float64]
        
        # Check values are reasonable (frequency should be positive)
        assert np.all(result_cpu >= 0)
        assert np.all(np.isfinite(result_cpu))
    
    def test_unwrap_hilbert_vs_cpu(self):
        """Test GPU version matches CPU version."""
        pytest.importorskip("vhsdecode.hilbert")
        from vhsdecode.demod import unwrap_hilbert
        
        # Create test signal
        freq_hz = 40e6
        n_samples = 2048
        phase = np.linspace(0, 8 * np.pi, n_samples)
        phase += np.random.randn(n_samples) * 0.1  # Add noise
        hilbert_cpu = np.exp(1j * phase).astype(np.complex128)
        
        # CPU version
        result_cpu = unwrap_hilbert(hilbert_cpu, freq_hz).real
        
        # GPU version
        hilbert_gpu = cp.asarray(hilbert_cpu)
        result_gpu = cp.asnumpy(unwrap_hilbert_gpu(hilbert_gpu, freq_hz))
        
        # Should match within tolerance
        # Allow some difference due to floating point operations
        np.testing.assert_allclose(result_cpu, result_gpu, rtol=1e-4, atol=1e-4)
    
    def test_unwrap_hilbert_with_wrapping(self):
        """Test with phase wrapping."""
        freq_hz = 40e6
        n_samples = 1024
        
        # Create signal with phase wrapping
        phase = np.zeros(n_samples)
        phase[:512] = np.linspace(0, 2 * np.pi, 512)
        phase[512:] = np.linspace(0, 2 * np.pi, 512)  # Wrap back
        
        hilbert_cpu = np.exp(1j * phase).astype(np.complex128)
        hilbert_gpu = cp.asarray(hilbert_cpu)
        
        result_gpu = unwrap_hilbert_gpu(hilbert_gpu, freq_hz)
        result_cpu = cp.asnumpy(result_gpu)
        
        # Should unwrap correctly
        assert np.all(np.isfinite(result_cpu))
        assert np.all(result_cpu >= 0)


@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestReplaceSpikeGPU:
    """Test GPU-accelerated spike replacement."""
    
    def test_replace_spikes_no_spikes(self):
        """Test when no spikes present."""
        n_samples = 1024
        demod_cpu = np.random.randn(n_samples).astype(np.float32) * 0.5
        demod_diffed_cpu = demod_cpu * 1.1
        
        demod_gpu = cp.asarray(demod_cpu)
        demod_diffed_gpu = cp.asarray(demod_diffed_cpu)
        
        result_gpu = replace_spikes_gpu(demod_gpu, demod_diffed_gpu, max_value=10.0)
        result_cpu = cp.asnumpy(result_gpu)
        
        # No spikes, so should be unchanged
        np.testing.assert_allclose(result_cpu, demod_cpu, rtol=1e-6)
    
    def test_replace_spikes_with_spikes(self):
        """Test spike replacement functionality."""
        n_samples = 1024
        demod_cpu = np.random.randn(n_samples).astype(np.float32) * 0.5
        demod_diffed_cpu = demod_cpu * 1.1
        
        # Add spikes
        spike_indices = [100, 500, 900]
        for idx in spike_indices:
            demod_cpu[idx] = 15.0  # Above threshold
        
        demod_gpu = cp.asarray(demod_cpu)
        demod_diffed_gpu = cp.asarray(demod_diffed_cpu)
        
        result_gpu = replace_spikes_gpu(
            demod_gpu, demod_diffed_gpu, max_value=10.0,
            replace_start=8, replace_end=30
        )
        result_cpu = cp.asnumpy(result_gpu)
        
        # Spikes should be replaced
        for idx in spike_indices:
            # Value at spike should be from diffed version
            assert result_cpu[idx] != 15.0
            assert np.isfinite(result_cpu[idx])
    
    def test_replace_spikes_vs_cpu(self):
        """Test GPU version matches CPU version."""
        pytest.importorskip("vhsdecode.demod")
        from vhsdecode.demod import replace_spikes
        
        n_samples = 2048
        demod_cpu = np.random.randn(n_samples).astype(np.float32)
        demod_diffed_cpu = demod_cpu * 0.9
        
        # Add spikes
        demod_cpu[200:205] = 20.0
        demod_cpu[1000] = 25.0
        
        max_value = 15.0
        
        # CPU version
        result_cpu_ref = replace_spikes(
            demod_cpu.copy(), demod_diffed_cpu,
            max_value, replace_start=8, replace_end=30
        )
        
        # GPU version
        demod_gpu = cp.asarray(demod_cpu)
        demod_diffed_gpu = cp.asarray(demod_diffed_cpu)
        result_gpu = replace_spikes_gpu(
            demod_gpu, demod_diffed_gpu,
            max_value, replace_start=8, replace_end=30
        )
        result_cpu = cp.asnumpy(result_gpu)
        
        # Should match
        np.testing.assert_allclose(result_cpu, result_cpu_ref, rtol=1e-5)


@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestEnvelopeFilterGPU:
    """Test GPU-accelerated envelope filtering."""
    
    def test_envelope_filter_basic(self):
        """Test basic envelope filtering."""
        n_samples = 1024
        raw_env_cpu = np.random.randn(n_samples).astype(np.float32) * 0.5 + 1.0
        
        # Simple filter (lowpass-like)
        filter_coeffs = np.ones(n_samples // 2 + 1, dtype=np.float32)
        filter_coeffs[100:] *= 0.1  # Attenuate high frequencies
        
        raw_env_gpu = cp.asarray(raw_env_cpu)
        
        result_gpu = envelope_filter_gpu(raw_env_gpu, filter_coeffs)
        result_cpu = cp.asnumpy(result_gpu)
        
        # Check output
        assert result_cpu.shape == raw_env_cpu.shape
        assert np.all(np.isfinite(result_cpu))
    
    def test_envelope_filter_preserves_dc(self):
        """Test that DC component is preserved."""
        n_samples = 1024
        dc_value = 5.0
        raw_env_cpu = np.ones(n_samples, dtype=np.float32) * dc_value
        
        # All-pass filter (should preserve DC)
        filter_coeffs = np.ones(n_samples // 2 + 1, dtype=np.float32)
        
        raw_env_gpu = cp.asarray(raw_env_cpu)
        result_gpu = envelope_filter_gpu(raw_env_gpu, filter_coeffs)
        result_cpu = cp.asnumpy(result_gpu)
        
        # DC should be preserved
        np.testing.assert_allclose(np.mean(result_cpu), dc_value, rtol=0.1)


@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestNonlinearDeemphasisGPU:
    """Test GPU-accelerated nonlinear deemphasis."""
    
    def test_nonlinear_deemphasis_basic(self):
        """Test basic nonlinear deemphasis."""
        n_samples = 1024
        out_video_cpu = np.random.randn(n_samples).astype(np.float32)
        
        # Create FFT
        out_video_fft_cpu = np.fft.rfft(out_video_cpu)
        
        # Highpass filter
        nl_highpass_filter = np.ones_like(out_video_fft_cpu, dtype=np.float32)
        nl_highpass_filter[:10] = 0.0  # Zero out low frequencies
        
        # Transfer to GPU
        out_video_gpu = cp.asarray(out_video_cpu)
        out_video_fft_gpu = cp.asarray(out_video_fft_cpu)
        nl_filter_gpu = cp.asarray(nl_highpass_filter)
        
        result_gpu = nonlinear_deemphasis_gpu(
            out_video_gpu, out_video_fft_gpu, nl_filter_gpu,
            limit_low=-2.0, limit_high=2.0
        )
        result_cpu = cp.asnumpy(result_gpu)
        
        # Check output
        assert result_cpu.shape == out_video_cpu.shape
        assert np.all(np.isfinite(result_cpu))
    
    def test_nonlinear_deemphasis_clipping(self):
        """Test that clipping is applied correctly."""
        n_samples = 512
        out_video_cpu = np.zeros(n_samples, dtype=np.float32)
        
        # Add high frequency component
        out_video_cpu += np.sin(np.linspace(0, 50 * np.pi, n_samples)) * 5.0
        
        out_video_fft_cpu = np.fft.rfft(out_video_cpu)
        
        # Highpass filter (passes high freq)
        nl_highpass_filter = np.zeros_like(out_video_fft_cpu, dtype=np.float32)
        nl_highpass_filter[50:] = 1.0
        
        out_video_gpu = cp.asarray(out_video_cpu)
        out_video_fft_gpu = cp.asarray(out_video_fft_cpu)
        nl_filter_gpu = cp.asarray(nl_highpass_filter)
        
        result_gpu = nonlinear_deemphasis_gpu(
            out_video_gpu, out_video_fft_gpu, nl_filter_gpu,
            limit_low=-1.0, limit_high=1.0
        )
        result_cpu = cp.asnumpy(result_gpu)
        
        # High frequency should be clipped and removed
        # Result should have smaller amplitude than input
        assert np.max(np.abs(result_cpu)) < np.max(np.abs(out_video_cpu))


@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestPhase2Integration:
    """Integration tests for Phase 2 optimizations."""
    
    def test_gpu_decoder_initialization_with_optimize_transfers(self):
        """Test that GPU decoder can be initialized with optimize_transfers."""
        pytest.importorskip("vhsdecode.process_gpu")
        from vhsdecode.process_gpu import VHSRFDecodeGPU
        
        # Should not raise
        decoder = VHSRFDecodeGPU(
            system="NTSC",
            tape_format="VHS",
            inputfreq=40,
            use_gpu=True,
            optimize_transfers=True
        )
        
        assert decoder.use_gpu
        assert decoder.optimize_transfers
        
        # Cleanup
        decoder.cleanup()
    
    def test_gpu_decoder_phase1_mode(self):
        """Test that Phase 1 mode can be enabled."""
        pytest.importorskip("vhsdecode.process_gpu")
        from vhsdecode.process_gpu import VHSRFDecodeGPU
        
        decoder = VHSRFDecodeGPU(
            system="NTSC",
            tape_format="VHS",
            inputfreq=40,
            use_gpu=True,
            optimize_transfers=False  # Phase 1 mode
        )
        
        assert decoder.use_gpu
        assert not decoder.optimize_transfers
        
        decoder.cleanup()


@pytest.mark.skipif(not GPU_AVAILABLE, reason="GPU not available")
class TestMemoryManagement:
    """Test GPU memory management in Phase 2."""
    
    def test_no_memory_leaks(self):
        """Test that GPU operations don't leak memory."""
        # Get initial free memory
        mempool = cp.get_default_memory_pool()
        mempool.free_all_blocks()
        initial_used = mempool.used_bytes()
        
        # Run operations multiple times
        for _ in range(10):
            n_samples = 1024
            data = cp.random.randn(n_samples, dtype=cp.float32)
            
            # Simulate some operations
            fft = cp.fft.fft(data)
            filtered = fft * 0.5
            result = cp.fft.ifft(filtered).real
            
            # Explicit cleanup
            del data, fft, filtered, result
        
        # Force cleanup
        mempool.free_all_blocks()
        final_used = mempool.used_bytes()
        
        # Should not have leaked significant memory
        assert final_used <= initial_used + 1024  # Allow small overhead


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
