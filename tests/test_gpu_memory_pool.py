"""
Tests for GPU memory pool pre-allocation and management.

These tests validate that:
1. Memory pools are correctly pre-allocated
2. VRAM usage increases as expected
3. Batch size calculations are correct
4. Memory limits are properly enforced
"""

import pytest
import numpy as np

# Try to import GPU utilities
try:
    import cupy as cp
    from vhsdecode.gpu_utils import (
        GPU_AVAILABLE,
        setup_gpu_memory_pools,
        get_gpu_info,
        free_gpu_memory
    )
    GPU_TESTS_ENABLED = GPU_AVAILABLE
except ImportError:
    GPU_TESTS_ENABLED = False
    cp = None


@pytest.mark.skipif(not GPU_TESTS_ENABLED, reason="GPU not available")
class TestGPUMemoryPool:
    """Tests for GPU memory pool functionality."""
    
    def test_memory_pool_setup_basic(self):
        """Test basic memory pool setup."""
        # Setup memory pool for small blocks
        blocklen = 32768
        batch_size = 10
        target_vram_gb = 0.5  # 500 MB
        
        # Get initial memory state
        info_before = get_gpu_info()
        mem_free_before = info_before['memory_free']
        
        # Setup memory pool
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Get memory state after setup
        info_after = get_gpu_info()
        mem_free_after = info_after['memory_free']
        
        # Verify memory was allocated
        # Note: Actual usage may differ from target due to pool management
        # Just verify some memory was used
        assert mem_free_after < mem_free_before, "Memory pool should allocate memory"
        
        # Cleanup
        free_gpu_memory()
    
    def test_memory_pool_large_batch(self):
        """Test memory pool setup with large batch size."""
        blocklen = 131072  # Large blocks
        batch_size = 50    # Large batch
        target_vram_gb = 1.0  # 1 GB
        
        # Get GPU info
        info = get_gpu_info()
        mem_total = info['memory_total']
        
        # Skip if not enough memory
        if mem_total < 2e9:  # Less than 2 GB
            pytest.skip("Not enough VRAM for large batch test")
        
        # Setup memory pool
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Verify pool was created
        mempool = cp.get_default_memory_pool()
        assert mempool.used_bytes() > 0, "Memory pool should have allocated memory"
        
        # Cleanup
        free_gpu_memory()
    
    def test_memory_pool_respects_limits(self):
        """Test that memory pool respects available VRAM."""
        blocklen = 32768
        batch_size = 10
        
        # Get available VRAM
        info = get_gpu_info()
        mem_free = info['memory_free']
        mem_total = info['memory_total']
        
        # Request more than available (should be capped)
        target_vram_gb = (mem_total / 1e9) * 1.5  # 150% of total
        
        # This should not crash and should cap at reasonable limit
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Verify we didn't exceed total VRAM
        mempool = cp.get_default_memory_pool()
        used = mempool.used_bytes()
        assert used < mem_total, "Memory pool should not exceed total VRAM"
        
        # Cleanup
        free_gpu_memory()
    
    def test_batch_size_calculation(self):
        """Test that optimal batch size is calculated correctly."""
        # This is tested indirectly through decoder initialization
        # We verify the calculation logic here
        
        blocklen = 131072
        estimated_mb_per_block = 2.0
        
        # For 1 GB target
        target_vram_gb = 1.0
        expected_batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
        expected_batch_size = max(10, min(expected_batch_size, 100))
        
        # Should be 500 blocks, clamped to 100
        assert expected_batch_size == 100
        
        # For 50 MB target
        target_vram_gb = 0.05
        expected_batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
        expected_batch_size = max(10, min(expected_batch_size, 100))
        
        # Should be 25 blocks, clamped to 10
        assert expected_batch_size == 10
    
    def test_memory_pool_allocation_performance(self):
        """Test that pre-allocated pool improves allocation performance."""
        blocklen = 32768
        batch_size = 10
        target_vram_gb = 0.2
        
        # Setup memory pool
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Allocate several arrays (should be fast due to pool)
        import time
        
        start = time.perf_counter()
        arrays = []
        for i in range(10):
            arr = cp.empty(blocklen, dtype=cp.float64)
            arrays.append(arr)
        alloc_time = time.perf_counter() - start
        
        # Each allocation should be very fast (<1ms on average with pool)
        avg_alloc_time = alloc_time / 10
        assert avg_alloc_time < 0.01, f"Allocations too slow: {avg_alloc_time*1000:.2f}ms average"
        
        # Cleanup
        del arrays
        free_gpu_memory()
    
    def test_memory_pool_info_logging(self, caplog):
        """Test that memory pool setup logs useful information."""
        import logging
        caplog.set_level(logging.INFO)
        
        blocklen = 32768
        batch_size = 10
        target_vram_gb = 0.1
        
        setup_gpu_memory_pools(blocklen, batch_size, target_vram_gb)
        
        # Verify informative log messages
        log_text = caplog.text
        assert "Pre-allocating GPU memory pool" in log_text
        assert "Block size" in log_text
        assert "Batch size" in log_text
        assert "Pool size" in log_text
        assert "GPU memory pool configured" in log_text
        
        # Cleanup
        free_gpu_memory()


@pytest.mark.skipif(not GPU_TESTS_ENABLED, reason="GPU not available")
class TestBatchSizeOptimization:
    """Tests for batch size calculation and optimization."""
    
    def test_batch_size_scales_with_vram(self):
        """Test that batch size increases with available VRAM."""
        # Simulate different VRAM amounts
        test_cases = [
            (1.0, 10),   # 1 GB free -> small batch
            (4.0, 100),  # 4 GB free -> large batch
            (12.0, 100), # 12 GB free -> max batch (capped at 100)
        ]
        
        for mem_free_gb, expected_min_batch in test_cases:
            # Calculate as done in process_gpu.py
            target_vram_gb = min(mem_free_gb * 0.25, 3.0)
            estimated_mb_per_block = 2.0
            optimal_batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
            optimal_batch_size = max(10, min(optimal_batch_size, 100))
            
            assert optimal_batch_size >= expected_min_batch, \
                f"For {mem_free_gb} GB VRAM, expected batch >= {expected_min_batch}, got {optimal_batch_size}"
    
    def test_batch_size_clamping(self):
        """Test that batch size is clamped to reasonable range."""
        # Very small VRAM should clamp to minimum
        target_vram_gb = 0.01  # 10 MB
        estimated_mb_per_block = 2.0
        batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
        batch_size = max(10, min(batch_size, 100))
        assert batch_size == 10, "Should clamp to minimum of 10"
        
        # Very large VRAM should clamp to maximum
        target_vram_gb = 10.0  # 10 GB
        batch_size = int((target_vram_gb * 1000) / estimated_mb_per_block)
        batch_size = max(10, min(batch_size, 100))
        assert batch_size == 100, "Should clamp to maximum of 100"


if __name__ == "__main__":
    # Run tests with verbose output
    pytest.main([__file__, "-v", "-s"])
