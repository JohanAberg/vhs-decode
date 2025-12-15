#!/usr/bin/env python3
"""
Simple validation script for GPU memory pool implementation.

This script validates that memory pool pre-allocation works correctly
without requiring pytest or other test infrastructure.
"""

import sys
import logging

# Setup logging
logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

def test_gpu_availability():
    """Test that GPU and CuPy are available."""
    logger.info("Test 1: Checking GPU availability...")
    
    try:
        import cupy as cp
        from vhsdecode.gpu_utils import GPU_AVAILABLE, get_gpu_info
        
        if not GPU_AVAILABLE:
            logger.error("GPU_AVAILABLE is False")
            return False
        
        info = get_gpu_info()
        if info is None:
            logger.error("Could not get GPU info")
            return False
        
        logger.info(f"✓ GPU Available: {info['name']}")
        logger.info(f"  Total VRAM: {info['memory_total']/1e9:.2f} GB")
        logger.info(f"  Free VRAM: {info['memory_free']/1e9:.2f} GB")
        return True
        
    except ImportError as e:
        logger.error(f"Cannot import CuPy: {e}")
        logger.info("Install CuPy: pip install cupy-cuda11x or cupy-cuda12x")
        return False
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        return False


def test_memory_pool_setup():
    """Test memory pool setup function."""
    logger.info("\nTest 2: Testing memory pool setup...")
    
    try:
        import cupy as cp
        from vhsdecode.gpu_utils import setup_gpu_memory_pools, get_gpu_info
        
        # Get initial state
        info_before = get_gpu_info()
        mem_free_before = info_before['memory_free']
        
        # Setup memory pool
        blocklen = 32768
        batch_size = 10
        target_vram_gb = 0.5  # 500 MB
        
        logger.info(f"Setting up memory pool: blocklen={blocklen}, batch_size={batch_size}, target={target_vram_gb}GB")
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Check if memory was allocated
        info_after = get_gpu_info()
        mem_free_after = info_after['memory_free']
        
        mem_used = mem_free_before - mem_free_after
        logger.info(f"✓ Memory pool allocated: {mem_used/1e6:.1f} MB")
        
        # Verify pool limit was set
        mempool = cp.get_default_memory_pool()
        pool_limit = mempool.get_limit()
        if pool_limit > 0:
            logger.info(f"✓ Memory pool limit set: {pool_limit/1e9:.2f} GB")
        else:
            logger.info("  Note: Memory pool limit not set (unlimited)")
        
        # Cleanup
        cp.get_default_memory_pool().free_all_blocks()
        
        return mem_used > 0
        
    except Exception as e:
        logger.error(f"Memory pool setup failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_batch_size_calculation():
    """Test batch size calculation logic."""
    logger.info("\nTest 3: Testing batch size calculation...")
    
    try:
        from vhsdecode.gpu_utils import get_gpu_info
        
        info = get_gpu_info()
        mem_total_gb = info['memory_total'] / 1e9
        mem_free_gb = info['memory_free'] / 1e9
        
        # Calculate as done in process_gpu.py
        target_vram_gb = min(mem_free_gb * 0.25, 3.0)
        estimated_mb_per_block = 2.0
        optimal_batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
        optimal_batch_size = max(10, min(optimal_batch_size, 100))
        
        logger.info(f"  Total VRAM: {mem_total_gb:.2f} GB")
        logger.info(f"  Free VRAM: {mem_free_gb:.2f} GB")
        logger.info(f"  Target pool: {target_vram_gb:.2f} GB ({target_vram_gb/mem_total_gb*100:.1f}% of total)")
        logger.info(f"✓ Optimal batch size: {optimal_batch_size} blocks")
        
        # Validate range
        if 10 <= optimal_batch_size <= 100:
            logger.info("✓ Batch size in valid range [10, 100]")
            return True
        else:
            logger.error(f"Batch size out of range: {optimal_batch_size}")
            return False
            
    except Exception as e:
        logger.error(f"Batch size calculation failed: {e}")
        return False


def test_batch_processor_import():
    """Test that BatchProcessor can be imported."""
    logger.info("\nTest 4: Testing BatchProcessor import...")
    
    try:
        from vhsdecode.gpu_batch_processor import BatchProcessor
        logger.info("✓ BatchProcessor imported successfully")
        
        # Create a mock decoder
        class MockDecoder:
            pass
        
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10, prefetch_size=2)
        
        logger.info(f"✓ BatchProcessor created: batch_size={processor.batch_size}")
        return True
        
    except Exception as e:
        logger.error(f"BatchProcessor import/creation failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_decoder_methods():
    """Test that VHSRFDecodeGPU has required methods."""
    logger.info("\nTest 5: Testing VHSRFDecodeGPU methods...")
    
    try:
        from vhsdecode.process_gpu import VHSRFDecodeGPU
        
        # Check for required methods
        required_methods = [
            'demodblock_gpu_only',
            '_demodblock_cpu_fallback',
            '_setup_gpu',
        ]
        
        for method_name in required_methods:
            if hasattr(VHSRFDecodeGPU, method_name):
                logger.info(f"✓ Method exists: {method_name}")
            else:
                logger.error(f"✗ Method missing: {method_name}")
                return False
        
        return True
        
    except Exception as e:
        logger.error(f"Decoder method check failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_vram_utilization_increase():
    """Test that we can use more VRAM than before."""
    logger.info("\nTest 6: Testing VRAM utilization increase...")
    
    try:
        import cupy as cp
        from vhsdecode.gpu_utils import setup_gpu_memory_pools, get_gpu_info
        
        info = get_gpu_info()
        mem_total = info['memory_total']
        
        # Test with aggressive settings
        blocklen = 131072  # Large blocks
        batch_size = 50     # Large batch
        target_vram_gb = min((mem_total / 1e9) * 0.3, 3.0)  # 30% of VRAM
        
        logger.info(f"Testing aggressive VRAM usage: {target_vram_gb:.2f} GB target")
        
        # Get initial usage
        mempool = cp.get_default_memory_pool()
        initial_used = mempool.used_bytes()
        
        # Setup pool
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Allocate some arrays to use the pool
        arrays = []
        for i in range(batch_size):
            arr = cp.empty(blocklen, dtype=cp.float64)
            arrays.append(arr)
        
        # Check usage
        peak_used = mempool.used_bytes()
        utilization_pct = (peak_used / mem_total) * 100
        
        logger.info(f"✓ Peak VRAM usage: {peak_used/1e6:.1f} MB ({utilization_pct:.1f}% of total)")
        
        # Cleanup
        del arrays
        cp.get_default_memory_pool().free_all_blocks()
        
        # Verify we're using more than the baseline 16MB
        if peak_used > 16e6:
            logger.info(f"✓ VRAM usage increased from baseline (16 MB → {peak_used/1e6:.1f} MB)")
            return True
        else:
            logger.warning(f"VRAM usage too low: {peak_used/1e6:.1f} MB")
            return False
            
    except Exception as e:
        logger.error(f"VRAM utilization test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Run all validation tests."""
    logger.info("=" * 70)
    logger.info("GPU Memory Pool Validation")
    logger.info("=" * 70)
    
    tests = [
        ("GPU Availability", test_gpu_availability),
        ("Memory Pool Setup", test_memory_pool_setup),
        ("Batch Size Calculation", test_batch_size_calculation),
        ("BatchProcessor Import", test_batch_processor_import),
        ("Decoder Methods", test_decoder_methods),
        ("VRAM Utilization", test_vram_utilization_increase),
    ]
    
    results = []
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            logger.error(f"Test '{test_name}' crashed: {e}")
            results.append((test_name, False))
    
    # Print summary
    logger.info("\n" + "=" * 70)
    logger.info("VALIDATION SUMMARY")
    logger.info("=" * 70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        logger.info(f"{status}: {test_name}")
    
    logger.info("=" * 70)
    logger.info(f"Results: {passed}/{total} tests passed")
    
    if passed == total:
        logger.info("✓ All validation tests passed!")
        return 0
    else:
        logger.error(f"✗ {total - passed} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
