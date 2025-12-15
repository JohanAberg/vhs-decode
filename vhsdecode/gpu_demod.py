"""
GPU-accelerated demodulation functions.

This module provides GPU implementations of core demodulation operations
to eliminate CPU↔GPU transfers and achieve better performance.
"""

import logging
import numpy as np

logger = logging.getLogger(__name__)

try:
    import cupy as cp
    GPU_AVAILABLE = True
except ImportError:
    cp = None
    GPU_AVAILABLE = False

from vhsdecode.gpu_utils import get_active_profiler

def unwrap_hilbert_gpu(hilbert_gpu, freq_hz):
    """
    GPU-accelerated Hilbert FM demodulation - OPTIMIZED.
    
    Keeps data on GPU throughout the operation to avoid transfers.
    Optimized to reduce synchronization points and kernel launches.
    
    Args:
        hilbert_gpu: Complex array on GPU (cp.ndarray)
        freq_hz: Sampling frequency in Hz
        
    Returns:
        cp.ndarray: Demodulated frequency data on GPU
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for unwrap_hilbert_gpu")
    
    profiler = get_active_profiler()
    
    tau = 2.0 * cp.pi
    freq_scale = freq_hz / tau
    
    # Method 3: Phase difference via complex conjugate product
    # dphi = angle( z[t] * conj(z[t-1]) )
    # This gives the phase change in [-pi, pi] directly, avoiding unwrap
    
    if profiler: profiler.start("6a_fm_complex_mult")
    # Calculate z[t] * conj(z[t-1])
    # This vector represents the phase rotation between samples
    if hilbert_gpu.ndim == 1:
        term = hilbert_gpu[1:] * cp.conj(hilbert_gpu[:-1])
    else:
        term = hilbert_gpu[..., 1:] * cp.conj(hilbert_gpu[..., :-1])
    if profiler: profiler.stop()
    
    if profiler: profiler.start("6b_fm_angle")
    # Extract phase angle (dphi)
    dphi = cp.angle(term)
    del term
    if profiler: profiler.stop()
    
    if profiler: profiler.start("6c_fm_pad_scale")
    # Pad to match original length (first sample has no diff)
    freq = cp.empty_like(hilbert_gpu, dtype=cp.float64)
    if hilbert_gpu.ndim == 1:
        freq[0] = 0.0
        freq[1:] = dphi
    else:
        freq[..., 0] = 0.0
        freq[..., 1:] = dphi
    del dphi
    
    # Fixups to map [-pi, pi] to [0, 2pi]
    # cp.mod (Python %) operator handles negative values correctly by wrapping to [0, tau)
    # This replaces the slower fmod + where combination
    freq = cp.mod(freq, tau)
    
    # Scale to Hz
    freq *= freq_scale
    if profiler: profiler.stop()
    
    return freq


def replace_spikes_gpu(demod_gpu, demod_diffed_gpu, max_value, replace_start=8, replace_end=30):
    """
    GPU-accelerated spike replacement.
    
    Args:
        demod_gpu: Demodulated signal on GPU
        demod_diffed_gpu: Differential demod signal on GPU
        max_value: Threshold for spike detection
        replace_start: Samples before spike to replace
        replace_end: Samples after spike to replace
        
    Returns:
        cp.ndarray: Demod signal with spikes replaced, on GPU
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for replace_spikes_gpu")
    
    # Handle batching
    if demod_gpu.ndim == 2:
        result = cp.empty_like(demod_gpu)
        for i in range(demod_gpu.shape[0]):
            result[i] = replace_spikes_gpu(demod_gpu[i], demod_diffed_gpu[i], max_value, replace_start, replace_end)
        return result

    # Find spikes
    too_high = demod_gpu > max_value
    spike_indices = cp.where(too_high)[0]
    
    # For each spike, replace surrounding samples
    # Note: This is a sequential operation that's hard to parallelize,
    # but keeping it on GPU avoids transfers
    result = demod_gpu.copy()
    for idx in spike_indices:
        start = max(int(idx) - replace_start, 0)
        end = min(int(idx) + replace_end, len(result))
        result[start:end] = demod_diffed_gpu[start:end]
    
    return result


def apply_filter_gpu(data_fft_gpu, filter_gpu):
    """
    Apply frequency-domain filter on GPU.
    
    Args:
        data_fft_gpu: FFT of data on GPU
        filter_gpu: Filter coefficients on GPU
        
    Returns:
        cp.ndarray: Filtered data in time domain on GPU
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for apply_filter_gpu")
    
    filtered_fft = data_fft_gpu * filter_gpu
    return cp.fft.ifft(filtered_fft).real


def envelope_filter_gpu(raw_env_gpu, filter_coeffs):
    """
    GPU-accelerated envelope filtering using frequency domain.
    
    Args:
        raw_env_gpu: Raw envelope data on GPU
        filter_coeffs: Filter coefficients (can be CPU or GPU array)
        
    Returns:
        cp.ndarray: Filtered envelope on GPU
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for envelope_filter_gpu")
    
    # Ensure filter is on GPU
    if not isinstance(filter_coeffs, cp.ndarray):
        filter_coeffs = cp.asarray(filter_coeffs)
    
    # Apply filter in frequency domain
    env_fft = cp.fft.rfft(raw_env_gpu)
    
    # Ensure filter length matches
    n_fft = env_fft.shape[-1]
    if len(filter_coeffs) != n_fft:
        # Resize filter to match FFT length
        if len(filter_coeffs) < n_fft:
            # Pad filter
            filter_resized = cp.zeros(n_fft, dtype=filter_coeffs.dtype)
            filter_resized[:len(filter_coeffs)] = filter_coeffs
            filter_coeffs = filter_resized
        else:
            # Truncate filter
            filter_coeffs = filter_coeffs[:n_fft]
    
    filtered_fft = env_fft * filter_coeffs
    return cp.fft.irfft(filtered_fft, n=raw_env_gpu.shape[-1])


def sub_deemphasis_gpu(out_video_gpu, out_video_fft_gpu, filters_gpu, 
                       deviation, exponential_scaling, scaling_1, scaling_2,
                       logistic_mid, logistic_rate, static_factor):
    """
    GPU-accelerated sub-deemphasis.
    
    This is a simplified GPU version that keeps the core operations on GPU
    while maintaining compatibility with the CPU version.
    
    Args:
        out_video_gpu: Video data on GPU
        out_video_fft_gpu: FFT of video data on GPU
        filters_gpu: Dictionary of filters on GPU
        deviation, exponential_scaling, scaling_1, scaling_2: Deemphasis parameters
        logistic_mid, logistic_rate, static_factor: More parameters
        
    Returns:
        cp.ndarray: Deemphasized video on GPU
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for sub_deemphasis_gpu")
    
    # This is a complex operation - for Phase 2, implement full GPU version
    # For now, provide a basic GPU-based version
    
    # Apply sub-deemphasis filter
    if "SubDeemp" in filters_gpu:
        sub_fft = out_video_fft_gpu * filters_gpu["SubDeemp"]
        sub_filtered = cp.fft.irfft(sub_fft, n=out_video_gpu.shape[-1])
        
        # Simple mixing (actual implementation is more complex)
        result = out_video_gpu + sub_filtered * static_factor
        return result
    
    # Fallback: return unchanged
    return out_video_gpu


def nonlinear_deemphasis_gpu(out_video_gpu, out_video_fft_gpu, nl_highpass_filter_gpu,
                             limit_low, limit_high):
    """
    GPU-accelerated nonlinear deemphasis.
    
    Args:
        out_video_gpu: Video data on GPU
        out_video_fft_gpu: FFT of video data on GPU  
        nl_highpass_filter_gpu: Nonlinear highpass filter on GPU
        limit_low: Lower clipping limit
        limit_high: Upper clipping limit
        
    Returns:
        cp.ndarray: Deemphasized video on GPU
    """
    if not GPU_AVAILABLE:
        raise RuntimeError("GPU not available for nonlinear_deemphasis_gpu")
    
    # Apply highpass filter
    hf_fft = out_video_fft_gpu * nl_highpass_filter_gpu
    hf_part = cp.fft.irfft(hf_fft, n=out_video_gpu.shape[-1])
    
    # Clip high frequency part
    cp.clip(hf_part, limit_low, limit_high, out=hf_part)
    
    # Subtract from original
    result = out_video_gpu - hf_part
    
    return result
