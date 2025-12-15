"""
CUDA kernels for VHS-Decode GPU acceleration.

This module contains custom CUDA kernels for optimizing specific operations
that benefit from fused implementations.
"""

__all__ = ['fused_fm_demod']
