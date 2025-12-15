"""
Filter conversion utilities for GPU acceleration.

This module provides functions to convert IIR filters to FIR approximations
that can be efficiently processed on GPU using FFT-based convolution.
"""

import numpy as np
import scipy.signal as sps
from typing import Tuple, Optional
import logging

logger = logging.getLogger(__name__)


def design_fir_envelope_filter(
    iir_sos: np.ndarray,
    sample_rate: float,
    blocklen: int,
    fir_length: int = 101
) -> Tuple[np.ndarray, np.ndarray]:
    """
    Convert IIR SOS filter to FIR approximation for GPU processing.
    
    The envelope filter (FEnvPost) is a 1st-order Butterworth lowpass at 700kHz.
    Since IIR filters require sequential time-domain processing (not parallelizable),
    we convert to FIR which can be applied via FFT multiplication on GPU.
    
    Args:
        iir_sos: IIR filter in Second-Order Sections format
        sample_rate: Sampling rate in Hz (typically 40 MHz)
        blocklen: FFT block size for frequency-domain representation
        fir_length: Length of FIR filter (odd number preferred for linear phase)
    
    Returns:
        fir_coeffs: Time-domain FIR filter coefficients
        fir_freq: Frequency-domain representation for GPU (matches blocklen)
    """
    # Get IIR frequency response at high resolution
    # Create frequency points from 0 to pi (0 to Nyquist)
    freq_points = 4096
    freq_normalized = np.linspace(0, 1, freq_points)  # 0 = DC, 1 = Nyquist
    w = freq_normalized * np.pi  # Convert to radians
    
    # Get IIR response at these frequency points
    _, h = sps.sosfreqz(iir_sos, worN=w)
    
    # Design FIR filter matching IIR magnitude response
    # Use firwin2 which designs filter from frequency samples
    fir_coeffs = sps.firwin2(
        fir_length,
        freq_normalized,
        abs(h),
        window='hamming'  # Good balance of main lobe width and side lobe suppression
    )
    
    # Compute frequency-domain representation for GPU FFT multiplication
    # Must match blocklen for overlap-save processing
    fir_freq = np.fft.rfft(fir_coeffs, n=blocklen)
    
    # Log filter characteristics
    logger.info(f"Converted IIR envelope filter to FIR:")
    logger.info(f"  FIR length: {fir_length} taps")
    logger.info(f"  Block length: {blocklen} samples")
    logger.info(f"  Frequency bins: {len(fir_freq)}")
    
    # Validate approximation quality
    w_fir_check, h_fir = sps.freqz(fir_coeffs, worN=4096)
    # Resample to match IIR response points for comparison
    mag_error = np.mean(np.abs(abs(h) - abs(h_fir)))
    
    if mag_error > 0.05:
        logger.warning(f"FIR approximation error is high: {mag_error:.4f}")
        logger.warning(f"Consider increasing fir_length (current: {fir_length})")
    else:
        logger.info(f"  Approximation error: {mag_error:.6f} (excellent)")
    
    return fir_coeffs, fir_freq


def validate_filter_conversion(
    iir_sos: np.ndarray,
    fir_coeffs: np.ndarray,
    sample_rate: float,
    plot_path: Optional[str] = None
) -> dict:
    """
    Validate FIR approximation quality against original IIR filter.
    
    Args:
        iir_sos: Original IIR filter in SOS format
        fir_coeffs: Converted FIR filter coefficients
        sample_rate: Sampling rate in Hz
        plot_path: Optional path to save comparison plot
    
    Returns:
        dict with error metrics: {'mag_error': float, 'phase_error': float, 'max_mag_error': float}
    """
    # Get frequency responses
    w_iir, h_iir = sps.sosfreqz(iir_sos, worN=4096)
    w_fir, h_fir = sps.freqz(fir_coeffs, worN=4096)
    
    # Convert angular frequency to Hz
    w_iir = w_iir * sample_rate / (2 * np.pi)
    w_fir = w_fir * sample_rate / (2 * np.pi)
    
    # Calculate error metrics
    mag_iir = abs(h_iir)
    mag_fir = abs(h_fir)
    phase_iir = np.angle(h_iir)
    phase_fir = np.angle(h_fir)
    
    metrics = {
        'mag_error_mean': np.mean(np.abs(mag_iir - mag_fir)),
        'mag_error_max': np.max(np.abs(mag_iir - mag_fir)),
        'mag_error_rms': np.sqrt(np.mean((mag_iir - mag_fir)**2)),
        'phase_error_mean': np.mean(np.abs(phase_iir - phase_fir)),
        'phase_error_max': np.max(np.abs(phase_iir - phase_fir)),
    }
    
    logger.info("Filter conversion validation metrics:")
    logger.info(f"  Magnitude error (mean): {metrics['mag_error_mean']:.6f}")
    logger.info(f"  Magnitude error (max):  {metrics['mag_error_max']:.6f}")
    logger.info(f"  Magnitude error (RMS):  {metrics['mag_error_rms']:.6f}")
    logger.info(f"  Phase error (mean):     {metrics['phase_error_mean']:.4f} rad")
    logger.info(f"  Phase error (max):      {metrics['phase_error_max']:.4f} rad")
    
    # Generate comparison plot if requested
    if plot_path:
        try:
            import matplotlib.pyplot as plt
            
            fig, axes = plt.subplots(2, 2, figsize=(14, 10))
            
            # Magnitude (linear)
            axes[0, 0].plot(w_iir / 1e6, mag_iir, 'b-', linewidth=2, label='IIR (original)')
            axes[0, 0].plot(w_fir / 1e6, mag_fir, 'r--', linewidth=1.5, label='FIR (approximation)')
            axes[0, 0].set_xlabel('Frequency (MHz)')
            axes[0, 0].set_ylabel('Magnitude')
            axes[0, 0].set_title('Magnitude Response (Linear)')
            axes[0, 0].legend()
            axes[0, 0].grid(True, alpha=0.3)
            
            # Magnitude (dB)
            axes[0, 1].plot(w_iir / 1e6, 20*np.log10(mag_iir + 1e-10), 'b-', linewidth=2, label='IIR')
            axes[0, 1].plot(w_fir / 1e6, 20*np.log10(mag_fir + 1e-10), 'r--', linewidth=1.5, label='FIR')
            axes[0, 1].set_xlabel('Frequency (MHz)')
            axes[0, 1].set_ylabel('Magnitude (dB)')
            axes[0, 1].set_title('Magnitude Response (dB)')
            axes[0, 1].set_ylim([-60, 5])
            axes[0, 1].legend()
            axes[0, 1].grid(True, alpha=0.3)
            
            # Phase
            axes[1, 0].plot(w_iir / 1e6, phase_iir, 'b-', linewidth=2, label='IIR')
            axes[1, 0].plot(w_fir / 1e6, phase_fir, 'r--', linewidth=1.5, label='FIR')
            axes[1, 0].set_xlabel('Frequency (MHz)')
            axes[1, 0].set_ylabel('Phase (radians)')
            axes[1, 0].set_title('Phase Response')
            axes[1, 0].legend()
            axes[1, 0].grid(True, alpha=0.3)
            
            # Error
            axes[1, 1].plot(w_iir / 1e6, np.abs(mag_iir - mag_fir), 'g-', linewidth=2)
            axes[1, 1].set_xlabel('Frequency (MHz)')
            axes[1, 1].set_ylabel('Magnitude Error')
            axes[1, 1].set_title('Approximation Error (Magnitude)')
            axes[1, 1].grid(True, alpha=0.3)
            axes[1, 1].set_yscale('log')
            
            plt.tight_layout()
            plt.savefig(plot_path, dpi=150, bbox_inches='tight')
            logger.info(f"Saved filter comparison plot to: {plot_path}")
            plt.close()
            
        except ImportError:
            logger.warning("matplotlib not available, skipping plot generation")
        except Exception as e:
            logger.warning(f"Failed to generate plot: {e}")
    
    return metrics
