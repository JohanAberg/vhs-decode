"""
GPU-accelerated RF decoder for VHS-Decode.

This module provides GPU-accelerated versions of the RF decoding pipeline
using CuPy for CUDA acceleration.
"""

import logging
import numpy as np

from vhsdecode.process import VHSRFDecode
from vhsdecode.gpu_utils import (
    GPU_AVAILABLE,
    get_array_module,
    transfer_to_gpu,
    transfer_from_gpu,
    free_gpu_memory,
    GPUError,
    GPUOutOfMemoryError
)

logger = logging.getLogger(__name__)

# Import CuPy if available
if GPU_AVAILABLE:
    import cupy as cp


class VHSRFDecodeGPU(VHSRFDecode):
    """
    GPU-accelerated RF decoder for VHS.
    
    This class extends VHSRFDecode to use GPU acceleration for
    computationally intensive operations like FFT and filtering.
    
    CPU Equivalent: VHSRFDecode in process.py
    Expected Speedup: 2-4x (Phase 1), 5-7x (Phase 2)
    Memory Required: ~2-4 MB VRAM per block
    
    Args:
        use_gpu (bool): Enable GPU acceleration (default: True if GPU available)
        gpu_id (int): GPU device ID to use (default: 0)
        **kwargs: All other arguments passed to VHSRFDecode
        
    Example:
        >>> decoder = VHSRFDecodeGPU(inputfreq=40, system="NTSC", use_gpu=True)
        >>> result = decoder.demodblock(rf_data)
    """
    
    def __init__(self, *args, use_gpu=True, gpu_id=0, **kwargs):
        """Initialize GPU-accelerated decoder."""
        # Initialize parent class first
        super().__init__(*args, **kwargs)
        
        # GPU configuration
        self.use_gpu = use_gpu and GPU_AVAILABLE
        self.gpu_id = gpu_id
        
        if not GPU_AVAILABLE and use_gpu:
            logger.warning(
                "GPU acceleration requested but CuPy not available. "
                "Falling back to CPU. Install CuPy: pip install cupy-cuda11x or cupy-cuda12x"
            )
            self.use_gpu = False
        
        if self.use_gpu:
            self._setup_gpu()
            logger.info(f"GPU acceleration enabled on device {self.gpu_id}")
        else:
            logger.info("GPU acceleration disabled, using CPU")
    
    def _setup_gpu(self):
        """Setup GPU device and transfer filters to GPU memory."""
        if not self.use_gpu:
            return
        
        try:
            # Set GPU device
            cp.cuda.Device(self.gpu_id).use()
            
            # Log GPU info
            device = cp.cuda.Device()
            logger.info(f"Using GPU: {device.name}")
            logger.info(f"VRAM: {device.mem_info[1]/1e9:.2f} GB total, "
                       f"{device.mem_info[0]/1e9:.2f} GB free")
            
            # Transfer filters to GPU
            self._move_filters_to_gpu()
            
        except Exception as e:
            logger.error(f"Failed to setup GPU: {e}")
            logger.warning("Falling back to CPU")
            self.use_gpu = False
    
    def _move_filters_to_gpu(self):
        """Transfer precomputed filters to GPU memory."""
        if not self.use_gpu:
            return
        
        self.Filters_gpu = {}
        
        try:
            for key, value in self.Filters.items():
                if isinstance(value, np.ndarray):
                    # Transfer numpy array to GPU
                    self.Filters_gpu[key] = cp.asarray(value)
                elif isinstance(value, tuple):
                    # Handle tuple of arrays (e.g., for scipy filters)
                    self.Filters_gpu[key] = value  # Keep on CPU for scipy operations
                else:
                    # Keep other types as-is
                    self.Filters_gpu[key] = value
            
            logger.debug(f"Transferred {len(self.Filters_gpu)} filters to GPU")
            
        except Exception as e:
            logger.error(f"Failed to transfer filters to GPU: {e}")
            raise GPUError(f"Filter transfer failed: {e}")
    
    def demodblock(
        self, data=None, mtf_level=0, fftdata=None, cut=False, thread_benchmark=False
    ):
        """
        GPU-accelerated demodulation block.
        
        Automatically falls back to CPU on GPU errors.
        
        Args:
            data: Input RF data block
            mtf_level: MTF compensation level
            fftdata: Pre-computed FFT data (optional)
            cut: Whether to cut block edges
            thread_benchmark: Enable benchmarking
            
        Returns:
            dict: Demodulated video data
            
        Raises:
            GPUError: If GPU processing fails (with CPU fallback)
        """
        if not self.use_gpu:
            # Use CPU version
            return super().demodblock(data, mtf_level, fftdata, cut, thread_benchmark)
        
        try:
            return self._demodblock_gpu(data, mtf_level, fftdata, cut, thread_benchmark)
        except (cp.cuda.memory.OutOfMemoryError, GPUError) as e:
            logger.warning(f"GPU demodblock failed: {e}, falling back to CPU")
            # Fallback to CPU
            free_gpu_memory()  # Clean up GPU memory
            return super().demodblock(data, mtf_level, fftdata, cut, thread_benchmark)
        except Exception as e:
            logger.error(f"Unexpected error in GPU demodblock: {e}")
            # Fallback to CPU
            free_gpu_memory()
            return super().demodblock(data, mtf_level, fftdata, cut, thread_benchmark)
    
    def _demodblock_gpu(
        self, data=None, mtf_level=0, fftdata=None, cut=False, thread_benchmark=False
    ):
        """
        Internal GPU-accelerated demodulation implementation.
        
        This is Phase 1 implementation focusing on FFT and filtering operations.
        Some operations still run on CPU and will be moved to GPU in Phase 2.
        """
        import time
        from vhsdecode import utils
        from vhsdecode.demod import unwrap_hilbert
        from vhsdecode.chroma import demod_chroma_filt
        from vhsdecode.nonlinear_filter import sub_deemphasis, replace_spikes
        import scipy.signal as sps
        
        # Use the full numpy.fft module reference for clearer code
        import numpy.fft as npfft
        
        rv = {}
        demod_start_time = time.time()
        
        # Phase 1: FFT operations on GPU
        if fftdata is not None:
            # Transfer to GPU if needed
            indata_fft_gpu = transfer_to_gpu(fftdata) if not isinstance(fftdata, cp.ndarray) else fftdata
        elif data is not None:
            # Transfer data to GPU and compute FFT
            data_gpu = transfer_to_gpu(data[:self.blocklen])
            indata_fft_gpu = cp.fft.fft(data_gpu)
            del data_gpu
        else:
            raise Exception("demodblock called without raw or FFT data")
        
        # Keep data on CPU for operations that aren't GPU-optimized yet
        if data is None:
            data = npfft.ifft(transfer_from_gpu(indata_fft_gpu)).real
        
        # Debug plot handling (CPU operation)
        if self.debug_plot and self.debug_plot.is_plot_requested("demodblock"):
            indata_fft_copy = transfer_from_gpu(indata_fft_gpu).copy()
        
        # Apply notch filter on GPU if needed
        if self._notch is not None:
            notch_filter_gpu = self.Filters_gpu["FVideoNotchF"]
            indata_fft_gpu *= notch_filter_gpu
        
        # Phase 1: Apply RF filters on GPU
        rf_filter_gpu = self.Filters_gpu["RFVideo"]
        indata_fft_gpu *= rf_filter_gpu
        
        # Hilbert envelope calculation (GPU accelerated)
        hilbert_filter_gpu = self.Filters_gpu["hilbert"]
        raw_filtered_gpu = cp.fft.ifft(indata_fft_gpu * hilbert_filter_gpu).real
        
        # Calculate envelope on GPU, then transfer for further CPU processing
        cp.abs(raw_filtered_gpu, out=raw_filtered_gpu)
        raw_env = cp.asnumpy(cp.roll(raw_filtered_gpu, 4))
        del raw_filtered_gpu
        
        # Envelope filtering (CPU for now, will move to GPU in Phase 2)
        env = utils.filter_simple(raw_env, self.Filters["FEnvPost"]).astype(np.single)
        del raw_env
        env_mean = np.mean(env)
        
        # High boost processing (CPU for now)
        if len(np.where(env == 0)[0]) == 0:
            if self._high_boost is not None:
                data_filtered = npfft.ifft(transfer_from_gpu(indata_fft_gpu)).real
                high_part = utils.filter_simple(
                    data_filtered, self.Filters["RFTop"]
                ) * ((env_mean * 0.9) / env)
                del data_filtered
                high_part_gpu = transfer_to_gpu(high_part)
                indata_fft_gpu += cp.fft.fft(high_part_gpu * self._high_boost)
                del high_part_gpu
        else:
            logger.warning("RF signal is weak. Is your deck tracking properly?")
        
        # Hilbert transform and FM demodulation
        hilbert_gpu = cp.fft.ifft(indata_fft_gpu * hilbert_filter_gpu)
        hilbert = transfer_from_gpu(hilbert_gpu)
        del hilbert_gpu
        
        # FM demodulator (CPU for now - Phase 2 will optimize)
        from vhsdecode.demod import unwrap_hilbert
        demod = unwrap_hilbert(hilbert, self.freq_hz).real
        
        # Differential demod for spike handling (CPU)
        if not self._disable_diff_demod:
            check_value = self.options.diff_demod_check_value
            if np.max(demod[20:-20]) > check_value:
                demod_b = unwrap_hilbert(
                    np.ediff1d(hilbert, to_begin=0), self.freq_hz
                ).real
                demod = replace_spikes(demod, demod_b, check_value)
                del demod_b
        
        # Video EQ (CPU)
        if self._video_eq:
            demod = self._video_eq.filter_video(demod)
        
        # Chroma trap (CPU)
        if self._chroma_trap:
            demod = self.chromaTrap.work(demod)
        
        # Phase 1: Deemphasis filtering on GPU
        demod_gpu = transfer_to_gpu(demod)
        demod_fft_gpu = cp.fft.rfft(demod_gpu)
        del demod_gpu
        
        # Apply video filter on GPU
        video_filter_gpu = self.Filters_gpu["FVideo"]
        out_video_fft_gpu = demod_fft_gpu * video_filter_gpu
        out_video_gpu = cp.fft.irfft(out_video_fft_gpu).real
        out_video = transfer_from_gpu(out_video_gpu)
        del out_video_gpu
        
        # Nonlinear deemphasis (CPU for now)
        if self.options.nldeemp:
            out_video_fft = transfer_from_gpu(out_video_fft_gpu)
            hf_part = npfft.irfft(out_video_fft * self.Filters["NLHighPassF"])
            np.clip(
                hf_part,
                self.DecoderParams["nonlinear_highpass_limit_l"],
                self.DecoderParams["nonlinear_highpass_limit_h"],
                out=hf_part,
            )
            out_video -= hf_part
        
        # Sub deemphasis (CPU)
        if self.options.subdeemp:
            out_video_fft = transfer_from_gpu(out_video_fft_gpu)
            out_video = sub_deemphasis(
                out_video,
                out_video_fft,
                self.Filters,
                self._sub_emphasis_params.deviation,
                self._sub_emphasis_params.exponential_scaling,
                self._sub_emphasis_params.scaling_1,
                self._sub_emphasis_params.scaling_2,
                self._sub_emphasis_params.logistic_mid,
                self._sub_emphasis_params.logistic_rate,
                self._sub_emphasis_params.static_factor,
            )
        
        del out_video_fft_gpu
        
        # FSC notch filter (CPU)
        if self._use_fsc_notch_filter:
            out_video = sps.filtfilt(
                self.Filters["fsc_notch"][0], self.Filters["fsc_notch"][1], out_video
            )
        
        # Video 0.5 filter on GPU
        video05_filter_gpu = self.Filters_gpu["FVideo05"]
        out_video05_gpu = cp.fft.irfft(demod_fft_gpu * video05_filter_gpu).real
        out_video05 = transfer_from_gpu(out_video05_gpu)
        del out_video05_gpu, demod_fft_gpu
        out_video05 = np.roll(out_video05, -self.Filters["F05_offset"])
        
        # Chroma processing (CPU for now)
        chroma_source = data if self.options.color_under else out_video
        out_chroma = (
            demod_chroma_filt(
                chroma_source,
                self.Filters["FVideoBurst"],
                self.blocklen,
                self.Filters["FVideoNotch"],
                self._notch,
                move=int(self.options.chroma_offset),
            )
            if not self._do_cafc
            else data[: self.blocklen]
        )
        
        # Debug plotting (CPU)
        if self.debug_plot and self.debug_plot.is_plot_requested("magdens"):
            from vhsdecode.debug_plot import plot_magnitude_density
            plot_magnitude_density(
                raw_data=data[: self.blocklen],
                filtered_data=npfft.ifft(transfer_from_gpu(indata_fft_gpu)).real,
                rfdecode=self,
            )
        
        if self.debug_plot and self.debug_plot.is_plot_requested("demodblock"):
            from vhsdecode.debug_plot import plot_input_data
            plot_input_data(
                raw_data=data,
                env=env,
                env_mean=env_mean,
                raw_fft=indata_fft_copy,
                filtered_fft=transfer_from_gpu(indata_fft_gpu),
                demod_video=demod,
                filtered_video=out_video,
                chroma=out_chroma,
                rf_filter=self.Filters["RFVideo"],
                rfdecode=self,
                plot_chroma_fft=True,
            )
        
        # Clean up GPU memory
        del indata_fft_gpu
        
        # Export raw TBC if requested
        if self.options.export_raw_tbc:
            out_video = demod
        
        # Prepare output
        video_out = np.rec.array(
            [out_video, out_video05, out_chroma, env],
            names=["demod", "demod_05", "demod_burst", "envelope"],
        )
        
        rv["video"] = (
            video_out[self.blockcut : -self.blockcut_end] if cut else video_out
        )
        
        demod_end_time = time.time()
        if thread_benchmark:
            rv["demod_time"] = demod_end_time - demod_start_time
        
        return rv
    
    def __del__(self):
        """Cleanup GPU resources on deletion."""
        if self.use_gpu:
            try:
                free_gpu_memory()
            except Exception:
                pass


# Convenience function for creating decoders with GPU support
def create_decoder(use_gpu=True, **kwargs):
    """
    Create a VHS RF decoder with optional GPU acceleration.
    
    Args:
        use_gpu (bool): Enable GPU acceleration if available
        **kwargs: Arguments passed to decoder constructor
        
    Returns:
        VHSRFDecodeGPU or VHSRFDecode: Decoder instance
        
    Example:
        >>> decoder = create_decoder(use_gpu=True, inputfreq=40, system="NTSC")
    """
    if use_gpu and GPU_AVAILABLE:
        return VHSRFDecodeGPU(use_gpu=True, **kwargs)
    else:
        return VHSRFDecode(**kwargs)
