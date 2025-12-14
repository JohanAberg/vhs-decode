"""
GPU utility functions for VHS-Decode.

This module handles GPU detection, array module selection, and GPU-related utilities.
"""

import logging
import sys

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
        
        # Safely get device name
        name = device.name
        if name is None:
            name = 'Unknown GPU'
        elif isinstance(name, bytes):
            name = name.decode('utf-8', errors='ignore')
        else:
            name = str(name)
        
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


def transfer_to_gpu(data):
    """
    Transfer numpy array to GPU if available.
    
    Args:
        data: numpy array or cupy array
        
    Returns:
        cupy array if GPU available, else original numpy array
    """
    if not GPU_AVAILABLE:
        return data
    
    # If already on GPU, return as-is
    if isinstance(data, cp.ndarray):
        return data
    
    # Transfer to GPU
    try:
        return cp.asarray(data)
    except Exception as e:
        logger.warning(f"Failed to transfer data to GPU: {e}")
        return data


def transfer_from_gpu(data):
    """
    Transfer array from GPU to CPU.
    
    Args:
        data: cupy array or numpy array
        
    Returns:
        numpy array
    """
    if not GPU_AVAILABLE:
        return data
    
    # If already on CPU, return as-is
    if not isinstance(data, cp.ndarray):
        return data
    
    # Transfer from GPU
    try:
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
