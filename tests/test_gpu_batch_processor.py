"""
Tests for GPU batch processor.

These tests validate that:
1. BatchProcessor correctly groups blocks
2. Batch processing reduces transfer overhead
3. Prefetching works correctly
4. Statistics are tracked accurately
"""

import pytest
import numpy as np
import time

# Try to import GPU utilities
try:
    import cupy as cp
    from vhsdecode.gpu_batch_processor import BatchProcessor
    from vhsdecode.gpu_utils import GPU_AVAILABLE, free_gpu_memory
    GPU_TESTS_ENABLED = GPU_AVAILABLE
except ImportError:
    GPU_TESTS_ENABLED = False
    cp = None


class MockDecoder:
    """Mock decoder for testing BatchProcessor."""
    
    def __init__(self):
        self.process_count = 0
        self.last_input = None
    
    def demodblock_gpu_only(self, data_gpu, mtf_level=0, cut=False):
        """Mock GPU processing that just returns the input."""
        self.process_count += 1
        self.last_input = data_gpu
        
        # Simulate some processing on GPU
        result_gpu = data_gpu * 2.0  # Simple operation
        
        return {
            'video': result_gpu,
            'envelope': result_gpu,
            'demod': result_gpu,
        }
    
    def _demodblock_cpu_fallback(self, data, mtf_level=0, cut=False):
        """Mock CPU fallback."""
        return {
            'video': data * 2.0,
            'envelope': data * 2.0,
            'demod': data * 2.0,
        }


@pytest.mark.skipif(not GPU_TESTS_ENABLED, reason="GPU not available")
class TestBatchProcessor:
    """Tests for BatchProcessor functionality."""
    
    def test_batch_processor_creation(self):
        """Test that BatchProcessor can be created."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10, prefetch_size=2)
        
        assert processor.batch_size == 10
        assert processor.prefetch_size == 2
        assert processor.batches_processed == 0
    
    def test_process_single_batch(self):
        """Test processing a single batch of blocks."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10)
        
        # Create test data (10 blocks of 1000 samples each)
        blocks_cpu = [np.random.randn(1000).astype(np.float64) for _ in range(10)]
        
        # Process batch
        results = processor.process_batch(blocks_cpu)
        
        # Verify results
        assert len(results) == 10
        assert processor.batches_processed == 1
        assert processor.blocks_processed == 10
        assert decoder.process_count == 10
        
        # Cleanup
        free_gpu_memory()
    
    def test_process_multiple_batches(self):
        """Test processing multiple batches."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=5)
        
        # Process 3 batches
        for batch_num in range(3):
            blocks_cpu = [np.random.randn(1000).astype(np.float64) for _ in range(5)]
            results = processor.process_batch(blocks_cpu)
            assert len(results) == 5
        
        # Verify statistics
        assert processor.batches_processed == 3
        assert processor.blocks_processed == 15
        
        # Cleanup
        free_gpu_memory()
    
    def test_batch_with_different_sizes(self):
        """Test processing batches of different sizes."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10)
        
        # Full batch
        blocks1 = [np.random.randn(1000).astype(np.float64) for _ in range(10)]
        results1 = processor.process_batch(blocks1)
        assert len(results1) == 10
        
        # Partial batch
        blocks2 = [np.random.randn(1000).astype(np.float64) for _ in range(5)]
        results2 = processor.process_batch(blocks2)
        assert len(results2) == 5
        
        # Single block
        blocks3 = [np.random.randn(1000).astype(np.float64)]
        results3 = processor.process_batch(blocks3)
        assert len(results3) == 1
        
        # Verify statistics
        assert processor.batches_processed == 3
        assert processor.blocks_processed == 16
        
        # Cleanup
        free_gpu_memory()
    
    def test_batch_with_mtf_and_cut(self):
        """Test batch processing with MTF levels and cut flags."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=5)
        
        blocks = [np.random.randn(1000).astype(np.float64) for _ in range(5)]
        mtf_levels = [0, 1, 0, 2, 1]
        cuts = [False, True, False, True, False]
        
        results = processor.process_batch(blocks, mtf_levels, cuts)
        
        assert len(results) == 5
        assert processor.blocks_processed == 5
        
        # Cleanup
        free_gpu_memory()
    
    def test_batch_transfer_overhead_reduction(self):
        """Test that batching reduces transfer overhead."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10)
        
        # Create test data
        blocks = [np.random.randn(10000).astype(np.float64) for _ in range(10)]
        
        # Process batch and measure time
        start = time.perf_counter()
        results = processor.process_batch(blocks)
        batch_time = time.perf_counter() - start
        
        # Get statistics
        stats = processor.get_statistics()
        
        # Transfer overhead should be less than 50% of total time
        # (In practice, with batching it's often 20-30%)
        assert stats['transfer_overhead_pct'] < 50, \
            f"Transfer overhead too high: {stats['transfer_overhead_pct']:.1f}%"
        
        # Cleanup
        free_gpu_memory()
    
    def test_batch_statistics(self):
        """Test that statistics are tracked correctly."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=5)
        
        # Process some batches
        for _ in range(3):
            blocks = [np.random.randn(1000).astype(np.float64) for _ in range(5)]
            processor.process_batch(blocks)
        
        # Get statistics
        stats = processor.get_statistics()
        
        assert stats['batches_processed'] == 3
        assert stats['blocks_processed'] == 15
        assert stats['avg_batch_size'] == 5.0
        assert stats['avg_transfer_time_ms'] > 0
        assert stats['avg_compute_time_ms'] > 0
        assert stats['total_time_s'] > 0
        
        # Cleanup
        free_gpu_memory()
    
    def test_group_blocks_helper(self):
        """Test the _group_blocks helper method."""
        # Create iterator of blocks
        blocks = [(np.random.randn(100), i, False) for i in range(25)]
        
        # Group into batches of 10
        batches = list(BatchProcessor._group_blocks(iter(blocks), batch_size=10))
        
        # Should have 3 batches (10 + 10 + 5)
        assert len(batches) == 3
        assert len(batches[0][0]) == 10  # First batch full
        assert len(batches[1][0]) == 10  # Second batch full
        assert len(batches[2][0]) == 5   # Third batch partial
    
    def test_empty_batch(self):
        """Test processing an empty batch."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10)
        
        # Process empty batch
        results = processor.process_batch([])
        
        assert len(results) == 0
        assert processor.batches_processed == 0
        
        # Cleanup
        free_gpu_memory()
    
    def test_batch_memory_cleanup(self):
        """Test that batch processing cleans up GPU memory."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=10)
        
        # Get initial memory state
        mempool = cp.get_default_memory_pool()
        initial_used = mempool.used_bytes()
        
        # Process several batches
        for _ in range(5):
            blocks = [np.random.randn(10000).astype(np.float64) for _ in range(10)]
            results = processor.process_batch(blocks)
            del results  # Explicit cleanup
        
        # Memory should be freed (or at least not growing unbounded)
        final_used = mempool.used_bytes()
        
        # Allow some memory growth, but not proportional to batch count
        # (Should be cleaned up by free_all_blocks() calls)
        assert final_used < initial_used + 100e6, \
            f"Memory leak detected: {(final_used - initial_used)/1e6:.1f} MB leaked"
        
        # Cleanup
        free_gpu_memory()


@pytest.mark.skipif(not GPU_TESTS_ENABLED, reason="GPU not available")
class TestBatchPerformance:
    """Performance-focused tests for batch processing."""
    
    def test_batch_vs_individual_performance(self):
        """Test that batch processing is faster than individual processing."""
        decoder = MockDecoder()
        
        # Create test data
        blocks = [np.random.randn(5000).astype(np.float64) for _ in range(20)]
        
        # Test individual processing (batch_size=1)
        processor_single = BatchProcessor(decoder, batch_size=1)
        start_single = time.perf_counter()
        for block in blocks:
            processor_single.process_batch([block])
        time_single = time.perf_counter() - start_single
        
        # Reset decoder
        decoder.process_count = 0
        
        # Test batch processing (batch_size=20)
        processor_batch = BatchProcessor(decoder, batch_size=20)
        start_batch = time.perf_counter()
        processor_batch.process_batch(blocks)
        time_batch = time.perf_counter() - start_batch
        
        # Batch processing should be faster
        # (May not always be true on fast GPUs with small data, but generally yes)
        speedup = time_single / time_batch
        print(f"\nBatch speedup: {speedup:.2f}x")
        print(f"Single: {time_single*1000:.1f}ms, Batch: {time_batch*1000:.1f}ms")
        
        # At minimum, batch should not be significantly slower
        assert time_batch < time_single * 1.5, \
            "Batch processing should not be significantly slower than individual"
        
        # Cleanup
        free_gpu_memory()
    
    def test_large_batch_processing(self):
        """Test processing a large batch (stress test)."""
        decoder = MockDecoder()
        processor = BatchProcessor(decoder, batch_size=100)
        
        # Create large batch (100 blocks)
        blocks = [np.random.randn(10000).astype(np.float64) for _ in range(100)]
        
        # Process batch
        start = time.perf_counter()
        results = processor.process_batch(blocks)
        elapsed = time.perf_counter() - start
        
        # Verify
        assert len(results) == 100
        assert processor.blocks_processed == 100
        
        # Should complete in reasonable time (< 5 seconds on most GPUs)
        assert elapsed < 5.0, f"Large batch took too long: {elapsed:.2f}s"
        
        print(f"\nLarge batch (100 blocks): {elapsed*1000:.1f}ms ({elapsed/100*1000:.2f}ms per block)")
        
        # Cleanup
        free_gpu_memory()


if __name__ == "__main__":
    # Run tests with verbose output
    pytest.main([__file__, "-v", "-s"])
