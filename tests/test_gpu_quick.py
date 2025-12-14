#!/usr/bin/env python3
"""
Quick validation script for GPU infrastructure.

This script can be run without full dependencies to verify
GPU detection and basic functionality.
"""

import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def test_gpu_utils_import():
    """Test that GPU utilities can be imported."""
    try:
        from vhsdecode import gpu_utils
        print("✓ GPU utilities module imported successfully")
        return True
    except Exception as e:
        print(f"✗ Failed to import GPU utilities: {e}")
        return False


def test_gpu_detection():
    """Test GPU detection."""
    try:
        from vhsdecode.gpu_utils import GPU_AVAILABLE, get_gpu_info, log_gpu_status
        
        print(f"\nGPU Detection:")
        print(f"  GPU Available: {GPU_AVAILABLE}")
        
        if GPU_AVAILABLE:
            print("\n  GPU Status:")
            log_gpu_status()
            
            info = get_gpu_info()
            if info:
                print(f"\n  GPU Details:")
                print(f"    Name: {info['name']}")
                print(f"    Compute Capability: {info['compute_capability']}")
                print(f"    Total VRAM: {info['memory_total']/1e9:.2f} GB")
                print(f"    Free VRAM: {info['memory_free']/1e9:.2f} GB")
        else:
            print("  (CuPy not installed or no CUDA GPU detected)")
            print("  Install CuPy: pip install cupy-cuda11x or cupy-cuda12x")
        
        print("✓ GPU detection test passed")
        return True
        
    except Exception as e:
        print(f"✗ GPU detection test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_gpu_decoder_import():
    """Test that GPU decoder can be imported."""
    try:
        from vhsdecode.process_gpu import VHSRFDecodeGPU
        print("✓ GPU decoder module imported successfully")
        return True
    except Exception as e:
        print(f"✗ Failed to import GPU decoder: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_array_module_selection():
    """Test array module selection."""
    try:
        from vhsdecode.gpu_utils import get_array_module, GPU_AVAILABLE
        
        # Test CPU mode
        xp_cpu = get_array_module(use_gpu=False)
        assert xp_cpu.__name__ == 'numpy', "CPU mode should return numpy"
        print("✓ Array module selection (CPU mode) works")
        
        # Test GPU mode
        xp_gpu = get_array_module(use_gpu=True)
        if GPU_AVAILABLE:
            assert xp_gpu.__name__ == 'cupy', "GPU mode should return cupy when available"
            print("✓ Array module selection (GPU mode) works")
        else:
            assert xp_gpu.__name__ == 'numpy', "GPU mode should fallback to numpy when GPU unavailable"
            print("✓ Array module selection (GPU fallback) works")
        
        return True
        
    except Exception as e:
        print(f"✗ Array module selection test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_basic_gpu_operations():
    """Test basic GPU operations if GPU is available."""
    try:
        from vhsdecode.gpu_utils import GPU_AVAILABLE, get_array_module
        
        if not GPU_AVAILABLE:
            print("⊘ Skipping GPU operations test (no GPU available)")
            return True
        
        import cupy as cp
        import numpy as np
        
        # Create test data
        test_data = np.array([1, 2, 3, 4, 5], dtype=np.float32)
        
        # Transfer to GPU
        gpu_data = cp.asarray(test_data)
        assert isinstance(gpu_data, cp.ndarray), "Should create CuPy array"
        
        # Perform operation on GPU
        gpu_result = gpu_data * 2
        
        # Transfer back
        cpu_result = cp.asnumpy(gpu_result)
        assert isinstance(cpu_result, np.ndarray), "Should convert back to NumPy"
        assert np.allclose(cpu_result, test_data * 2), "Results should match"
        
        print("✓ Basic GPU operations test passed")
        return True
        
    except Exception as e:
        print(f"✗ Basic GPU operations test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Run all quick validation tests."""
    print("=" * 60)
    print("GPU Infrastructure Quick Validation")
    print("=" * 60)
    
    tests = [
        ("GPU Utils Import", test_gpu_utils_import),
        ("GPU Detection", test_gpu_detection),
        ("GPU Decoder Import", test_gpu_decoder_import),
        ("Array Module Selection", test_array_module_selection),
        ("Basic GPU Operations", test_basic_gpu_operations),
    ]
    
    results = []
    for name, test_func in tests:
        print(f"\n{name}:")
        print("-" * 60)
        results.append(test_func())
    
    print("\n" + "=" * 60)
    print("Summary:")
    print("=" * 60)
    passed = sum(results)
    total = len(results)
    print(f"Tests passed: {passed}/{total}")
    
    if passed == total:
        print("\n✓ All quick validation tests passed!")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
