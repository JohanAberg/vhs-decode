#!/usr/bin/env python3
"""
Test script to verify the memory pool limit fix logic without GPU.

This simulates the scenario that caused the issue in validate_memory_pool.py
"""

class MockMempool:
    """Mock CuPy memory pool for testing."""
    def __init__(self):
        self.limit = 0
        self.allocations = []
        
    def set_limit(self, size):
        self.limit = size
        print(f"  [Mock] Setting pool limit to {size/1e9:.2f} GB")
        
    def get_limit(self):
        return self.limit
        
    def used_bytes(self):
        return sum(self.allocations)
        
    def free_all_blocks(self):
        self.allocations.clear()
        print(f"  [Mock] Freed all blocks")
        
    def allocate(self, size):
        if self.limit > 0 and size > self.limit:
            raise RuntimeError(f"Out of memory allocating {size:,} bytes (limit set to: {self.limit:,} bytes)")
        self.allocations.append(size)
        print(f"  [Mock] Allocated {size/1e6:.1f} MB")


def test_scenario():
    """Test the scenario that caused the issue."""
    print("="*70)
    print("Testing Memory Pool Limit Fix")
    print("="*70)
    
    # Simulate total VRAM
    mem_total = 12e9  # 12 GB
    
    # Create mock mempool
    mempool = MockMempool()
    
    print("\n1. Test 2: Small allocation (500 MB)")
    print("-"*70)
    
    # Test 2 allocates 500 MB and sets limit to 1 GB
    pool_size_test2 = 500e6
    pool_limit_test2 = pool_size_test2 * 2  # 1 GB
    
    # Allocate
    try:
        mempool.allocate(pool_size_test2)
        mempool.set_limit(pool_limit_test2)
        print(f"✓ Test 2 succeeded: allocated {pool_size_test2/1e6:.0f} MB, limit {pool_limit_test2/1e9:.2f} GB")
    except Exception as e:
        print(f"✗ Test 2 failed: {e}")
        return False
    
    # Cleanup (but limit persists in CuPy!)
    mempool.free_all_blocks()
    print(f"  Current limit after cleanup: {mempool.get_limit()/1e9:.2f} GB")
    
    print("\n2. Test 6 (OLD - without fix): Large allocation (3 GB)")
    print("-"*70)
    
    # Test 6 tries to allocate 3 GB - this would fail with old code
    pool_size_test6 = 3e9
    
    try:
        mempool.allocate(pool_size_test6)
        print(f"✗ OLD CODE: Should have failed but didn't")
        return False
    except Exception as e:
        print(f"✓ OLD CODE correctly fails: {e}")
    
    mempool.free_all_blocks()
    
    print("\n3. Test 6 (NEW - with fix): Large allocation (3 GB)")
    print("-"*70)
    
    # NEW CODE: Calculate final limit upfront, set it BEFORE allocation
    print("  Applying fix: Calculate final limit and set BEFORE allocation...")
    
    # Calculate final pool limit upfront (same logic as gpu_utils.py)
    pool_limit_test6 = min(pool_size_test6 * 2, int(mem_total * 0.9))
    
    current_limit = mempool.get_limit()
    if current_limit > 0 and pool_size_test6 > current_limit:
        print(f"  Current limit ({current_limit/1e9:.2f} GB) too low for {pool_size_test6/1e9:.2f} GB allocation")
        print(f"  Increasing pool limit to {pool_limit_test6/1e9:.2f} GB BEFORE allocation...")
        mempool.set_limit(pool_limit_test6)
    elif current_limit == 0:
        print(f"  No limit set yet, setting to {pool_limit_test6/1e9:.2f} GB...")
        mempool.set_limit(pool_limit_test6)
    
    try:
        mempool.allocate(pool_size_test6)
        print(f"✓ NEW CODE succeeds: allocated {pool_size_test6/1e9:.2f} GB")
        print(f"  Final limit: {pool_limit_test6/1e9:.2f} GB")
        
    except Exception as e:
        print(f"✗ NEW CODE failed: {e}")
        return False
    
    print("\n" + "="*70)
    print("✓ All tests passed - fix works correctly!")
    print("="*70)
    return True


if __name__ == "__main__":
    import sys
    success = test_scenario()
    sys.exit(0 if success else 1)
