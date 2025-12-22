"""GPU-accelerated chroma TBC helpers.

This module mirrors the logic of :func:`lddecode.utils.scale_field` but executes
on the GPU using CuPy so the chroma TBC step can avoid the CPU bottleneck.
"""

from __future__ import annotations

import numpy as np

from vhsdecode.gpu_utils import GPU_AVAILABLE, GPUError

if GPU_AVAILABLE:
    import cupy as cp  # type: ignore
else:  # pragma: no cover - executed when GPU is unavailable
    cp = None  # type: ignore

__all__ = ["chroma_tbc_gpu", "scale_field_gpu"]


def _compute_level_adjustments(wowfactors_gpu: "cp.ndarray", threshold_factor: float) -> "cp.ndarray":
    """Match the CPU-side MAD based level adjustment calculation on GPU."""
    median = cp.median(wowfactors_gpu)
    abs_dev = cp.abs(wowfactors_gpu - median)
    mad = cp.median(abs_dev)
    mad_value = float(mad.get()) if hasattr(mad, "get") else float(mad)
    threshold_value = threshold_factor * mad_value if mad_value > 0 else 0.001
    threshold = cp.float32(threshold_value)
    return cp.where(abs_dev > threshold, median, wowfactors_gpu)


def scale_field_gpu(
    buf: np.ndarray,
    interpolated_pixel_locs: np.ndarray,
    wowfactors: np.ndarray,
    lineoffset: int,
    outwidth: int,
    linesout: int,
    level_adjust_threshold: float = 15,
) -> np.ndarray:
    """Scale a demodulated chroma buffer using the GPU.

    Args:
        buf: Source signal (typically ``field.data["video"]["demod_burst"]``)
        interpolated_pixel_locs: Output of ``field.computewow_scaled()[0]``
        wowfactors: Output of ``field.computewow_scaled()[1]``
        lineoffset: Field line offset used by TBC
        outwidth: Output line length
        linesout: Number of lines to output
        level_adjust_threshold: Matches CPU logic (defaults to 15)

    Returns:
        ``np.float32`` array containing the scaled chroma field
    """
    if not GPU_AVAILABLE:  # pragma: no cover - guarded by CLI flag already
        raise GPUError("GPU chroma TBC requested but GPU support is unavailable")

    buf_gpu = cp.asarray(buf, dtype=cp.float32)
    interpolated_gpu = cp.asarray(interpolated_pixel_locs, dtype=cp.float32)
    wowfactors_gpu = cp.asarray(wowfactors, dtype=cp.float32)

    total_samples = int(linesout * outwidth)
    lineoffset_out_samples = int((lineoffset + 1) * outwidth)
    start = lineoffset_out_samples
    stop = start + total_samples

    if stop > interpolated_gpu.size:
        raise GPUError(
            "Interpolated pixel locations shorter than expected output field length"
        )

    sample_idx = cp.arange(start, stop, dtype=cp.int32)
    coords = interpolated_gpu[sample_idx]
    coord_int = cp.floor(coords).astype(cp.int32)
    x = coords - coord_int.astype(cp.float32)

    max_index = buf_gpu.size - 1
    idx0 = cp.clip(coord_int - 1, 0, max_index)
    idx1 = cp.clip(coord_int, 0, max_index)
    idx2 = cp.clip(coord_int + 1, 0, max_index)
    idx3 = cp.clip(coord_int + 2, 0, max_index)

    p0 = buf_gpu[idx0]
    p1 = buf_gpu[idx1]
    p2 = buf_gpu[idx2]
    p3 = buf_gpu[idx3]

    level_adjusts = _compute_level_adjustments(wowfactors_gpu, level_adjust_threshold)
    level_adjust = level_adjusts[sample_idx]

    point_5 = cp.float32(0.5)
    two = cp.float32(2.0)
    three = cp.float32(3.0)
    four = cp.float32(4.0)
    five = cp.float32(5.0)

    a = p2 - p0
    b = two * p0 - five * p1 + four * p2 - p3
    c = three * (p1 - p2) + p3 - p0

    out_gpu = level_adjust * (p1 + point_5 * x * (a + x * (b + x * c)))
    return cp.asnumpy(out_gpu)


def chroma_tbc_gpu(
    field,
    channel: str = "demod_burst",
    level_adjust_threshold: float = 15,
) -> np.ndarray:
    """GPU equivalent of ``ldd.Field.downscale(..., channel="demod_burst")``.

    Args:
        field: Field instance containing demodulated data
        channel: Video channel to resample (defaults to ``demod_burst``)
        level_adjust_threshold: Threshold multiplier for the MAD logic

    Returns:
        ``np.float32`` chroma buffer ready for subsequent processing
    """
    if not GPU_AVAILABLE:  # pragma: no cover
        raise GPUError("GPU chroma TBC requested but GPU support is unavailable")

    buf = field.data["video"][channel].astype(np.float32, copy=False)
    interpolated_pixel_locs, wowfactors = field.computewow_scaled()
    return scale_field_gpu(
        buf,
        interpolated_pixel_locs,
        wowfactors,
        field.lineoffset,
        field.outlinelen,
        field.outlinecount,
        level_adjust_threshold=level_adjust_threshold,
    )
