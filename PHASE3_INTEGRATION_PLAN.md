# Phase 3: Batch Processing Integration Plan

**Date:** December 15, 2025  
**Status:** Planning Phase  
**Complexity:** High - Requires DemodCache Architecture Changes

---

## Overview

Phase 3 aims to integrate the BatchProcessor with the main decode loop to reduce PCIe transfer overhead from 20.9% to 5-10% and increase FPS from 2.49 to ~4.2.

**Current Status:**
- ✅ BatchProcessor infrastructure complete (Phase 2)
- ✅ Memory pools configured (Phase 1)
- ⏸️ Integration with DemodCache pending (complex architectural change)

---

## Architecture Analysis

### Current DemodCache Architecture

The DemodCache uses a worker thread pool pattern:

```python
# lddecode/core.py - DemodCache

class DemodCache:
    def __init__(self, num_worker_threads=6):
        self.q_in = Queue()          # Input queue for work items
        self.q_out = Queue()         # Output queue for results
        self.blocks = {}             # Cache of processed blocks
        
        # Start worker threads
        for i in range(num_worker_threads):
            t = threading.Thread(target=self.worker, daemon=True)
            t.start()
            
    def worker(self):
        """Worker thread that processes blocks."""
        while True:
            item = self.q_in.get()
            if item is None:
                return
                
            blocknum, block, target_MTF, request = item[1:]
            
            # Process single block
            output["demod"] = rf.demodblock(
                data=block["rawinput"], 
                fftdata=fftdata, 
                mtf_level=target_MTF, 
                cut=True
            )
            
            self.q_out.put((blocknum, output))
```

**Key Observations:**
1. **Individual block processing**: Each worker processes one block at a time
2. **Multiple worker threads**: 6 workers by default (good for CPU, less optimal for GPU)
3. **Queue-based coordination**: Work items distributed via queues
4. **Block independence**: Each block processed separately

### BatchProcessor Design

The BatchProcessor groups multiple blocks:

```python
# vhsdecode/gpu_batch_processor.py

class BatchProcessor:
    def process_batch(self, blocks_cpu):
        """Process multiple blocks together."""
        # Transfer entire batch to GPU
        blocks_gpu = [cp.asarray(block) for block in blocks_cpu]
        
        # Process each on GPU (data stays in VRAM)
        results_gpu = []
        for block_gpu in blocks_gpu:
            result_gpu = decoder.demodblock_gpu_only(block_gpu)
            results_gpu.append(result_gpu)
        
        # Transfer entire batch back
        results_cpu = [cp.asnumpy(result) for result in results_gpu]
        
        return results_cpu
```

**Key Features:**
1. **Batch transfer**: Single transfer for multiple blocks
2. **GPU-resident processing**: Data stays on GPU during batch
3. **Amortized latency**: PCIe overhead spread over batch size

---

## Integration Challenges

### Challenge 1: Thread Pool vs Batch Processing

**Problem:** DemodCache uses 6 CPU worker threads, but GPU benefits from fewer threads with larger batches.

**Current:**
```
Thread 1: Block 1 → GPU → CPU
Thread 2: Block 2 → GPU → CPU
Thread 3: Block 3 → GPU → CPU
...
Thread 6: Block 6 → GPU → CPU

= 6 separate GPU transfers (high overhead)
```

**Ideal:**
```
Batch Thread: Blocks 1-10 → GPU → Process batch → CPU

= 1 GPU transfer for 10 blocks (low overhead)
```

**Solution Options:**

**Option A: GPU-Specific Worker (Recommended)**
```python
class DemodCache:
    def __init__(self, use_gpu=False, batch_size=10):
        if use_gpu:
            # Single GPU worker with batch processing
            self.num_worker_threads = 1
            self.batch_processor = BatchProcessor(rf, batch_size)
            t = threading.Thread(target=self.gpu_batch_worker, daemon=True)
        else:
            # Multiple CPU workers
            self.num_worker_threads = 6
            for i in range(self.num_worker_threads):
                t = threading.Thread(target=self.worker, daemon=True)
        t.start()
```

**Option B: Hybrid Approach**
```python
class DemodCache:
    def __init__(self, use_gpu=False, batch_size=10):
        # Always use thread pool, but batch on GPU
        self.num_worker_threads = 6
        self.batch_processor = BatchProcessor(rf, batch_size) if use_gpu else None
        
        for i in range(self.num_worker_threads):
            t = threading.Thread(target=self.worker, daemon=True)
            t.start()
            
    def worker(self):
        """Worker with optional batching."""
        if self.batch_processor and self.accumulated_blocks >= self.batch_size:
            # Process batch
            results = self.batch_processor.process_batch(self.accumulated_blocks)
            # ... handle results
        else:
            # Process single block (CPU or GPU)
            result = rf.demodblock(...)
```

### Challenge 2: Block Order Preservation

**Problem:** Batching must preserve block order for correct field assembly.

**Solution:** Track block numbers and reassemble in order:
```python
def gpu_batch_worker(self):
    batch_items = []
    batch_blocks = []
    
    # Accumulate batch
    while len(batch_items) < self.batch_size:
        item = self.q_in.get()
        if item is None:
            break
        batch_items.append(item)
        batch_blocks.append(item[2]["rawinput"])  # Raw RF data
    
    # Process batch
    results = self.batch_processor.process_batch(batch_blocks)
    
    # Output in order
    for item, result in zip(batch_items, results):
        blocknum = item[1]
        self.q_out.put((blocknum, result))
```

### Challenge 3: Cache Invalidation

**Problem:** Cached blocks may need reprocessing with different MTF levels.

**Current Behavior:**
- DemodCache caches processed blocks
- If MTF changes, blocks are reprocessed
- With batching, this becomes more complex

**Solution:** Batch-aware cache management:
```python
def doread(self, blocknums, MTF, redo=False):
    # Group blocks that need processing
    needs_processing = []
    
    for b in blocknums:
        if redo or "demod" not in self.blocks[b]:
            needs_processing.append(b)
    
    # Process in batches
    if self.batch_processor and len(needs_processing) >= self.batch_size:
        self.process_batch(needs_processing)
    else:
        # Process individually
        for b in needs_processing:
            self.process_single(b)
```

---

## Implementation Approach

### Phase 3A: Minimal Integration (Low Risk)

**Objective:** Add batch processing option without breaking existing code.

**Changes:**
1. Add `use_batch_processing` flag to DemodCache
2. Create separate `gpu_batch_worker()` method
3. Use GPU batch worker when flag is set
4. Fallback to existing worker for CPU or non-batch mode

**Estimated Effort:** 2-3 days  
**Risk:** Low (existing code unchanged)  
**Expected Gain:** +20% performance (transfer overhead reduction)

**Implementation:**
```python
class DemodCache:
    def __init__(self, ..., use_batch_processing=False, batch_size=10):
        self.use_batch_processing = use_batch_processing
        self.batch_size = batch_size
        
        if use_batch_processing and hasattr(rf, 'use_gpu') and rf.use_gpu:
            # GPU batch mode: single worker
            self.num_worker_threads = 1
            from vhsdecode.gpu_batch_processor import BatchProcessor
            self.batch_processor = BatchProcessor(rf, batch_size)
            t = threading.Thread(target=self.gpu_batch_worker, daemon=True)
            t.start()
        else:
            # Standard mode: multiple workers
            self.num_worker_threads = num_worker_threads
            for i in range(self.num_worker_threads):
                t = threading.Thread(target=self.worker, daemon=True)
                t.start()
    
    def gpu_batch_worker(self):
        """GPU-specific worker that processes blocks in batches."""
        batch_items = []
        batch_blocks = []
        batch_params = []
        
        while True:
            # Accumulate batch
            while len(batch_items) < self.batch_size:
                try:
                    item = self.q_in.get(timeout=0.1)
                except Empty:
                    # Timeout - process what we have
                    break
                    
                if item is None or item[0] == "END":
                    # Process remaining and exit
                    break
                    
                if item[0] == "DEMOD":
                    blocknum, block, target_MTF, request = item[1:]
                    batch_items.append((blocknum, block, target_MTF, request))
                    batch_blocks.append(block["rawinput"])
                    batch_params.append((target_MTF, request))
            
            if not batch_items:
                continue
                
            # Process batch
            try:
                results = self.batch_processor.process_batch(
                    batch_blocks,
                    mtf_levels=[p[0] for p in batch_params],
                    cuts=[True] * len(batch_items)
                )
                
                # Output results in order
                for (blocknum, block, mtf, request), result in zip(batch_items, results):
                    output = {}
                    output["demod"] = result
                    output["MTF"] = mtf
                    output["request"] = request
                    self.q_out.put((blocknum, output))
                    
            except Exception as e:
                # Fallback to individual processing
                logger.error(f"Batch processing failed: {e}, falling back to individual")
                for blocknum, block, mtf, request in batch_items:
                    # Process individually using CPU fallback
                    output = {}
                    output["demod"] = self.rf.demodblock(
                        data=block["rawinput"],
                        mtf_level=mtf,
                        cut=True
                    )
                    output["MTF"] = mtf
                    output["request"] = request
                    self.q_out.put((blocknum, output))
            
            # Clear batch
            batch_items = []
            batch_blocks = []
            batch_params = []
```

### Phase 3B: Full Integration (High Risk)

**Objective:** Fully optimize for batch processing throughout decode loop.

**Changes:**
1. Redesign DemodCache for batch-first processing
2. Prefetch multiple blocks at once
3. Batch-aware LRU cache
4. Optimize queue management for batches

**Estimated Effort:** 1-2 weeks  
**Risk:** High (major architectural change)  
**Expected Gain:** +30-40% performance

**Recommendation:** Start with Phase 3A, validate performance, then consider 3B.

---

## Testing Strategy

### Unit Tests

Add tests for batch-aware DemodCache:

```python
# tests/test_demodcache_batching.py

def test_batch_worker_ordering():
    """Test that batch worker preserves block order."""
    cache = DemodCache(rf, ..., use_batch_processing=True, batch_size=10)
    
    # Queue 20 blocks
    for i in range(20):
        cache.q_in.put(("DEMOD", i, {"rawinput": data[i]}, 1.0, 0))
    
    # Collect results
    results = []
    for i in range(20):
        blocknum, output = cache.q_out.get()
        results.append((blocknum, output))
    
    # Verify order
    assert [r[0] for r in results] == list(range(20))
```

### Integration Tests

Test with real decode:

```bash
# Test GPU batch mode
python decode.py vhs --system PAL --gpu --batch-size 10 --length 100 input.u8 output

# Compare with non-batch mode
python decode.py vhs --system PAL --gpu --no-batching --length 100 input.u8 output_nobatch

# Verify output matches
python compare_tbc.py output.tbc output_nobatch.tbc
```

### Performance Benchmarking

Measure impact of different batch sizes:

```python
# benchmark_batching.py

batch_sizes = [1, 5, 10, 20, 50, 100]
results = {}

for batch_size in batch_sizes:
    fps = benchmark_decode(batch_size=batch_size)
    results[batch_size] = fps
    print(f"Batch size {batch_size}: {fps:.2f} FPS")

# Expected results:
# Batch 1:   2.49 FPS (baseline)
# Batch 5:   3.20 FPS (+28%)
# Batch 10:  3.80 FPS (+53%)
# Batch 20:  4.10 FPS (+65%)
# Batch 50:  4.20 FPS (+69%) ← diminishing returns
# Batch 100: 4.25 FPS (+71%)
```

---

## Rollout Plan

### Stage 1: Preparation (Complete)
- ✅ BatchProcessor implementation
- ✅ Memory pool setup
- ✅ Documentation

### Stage 2: Development (Current)
- [ ] Implement `gpu_batch_worker()` method
- [ ] Add `use_batch_processing` flag
- [ ] Update CLI to expose batch size option
- [ ] Add unit tests

### Stage 3: Testing (Next)
- [ ] Test with sample captures
- [ ] Validate block ordering
- [ ] Compare output accuracy
- [ ] Measure performance gains

### Stage 4: Optimization (After testing)
- [ ] Tune batch size per system
- [ ] Add automatic batch size selection
- [ ] Optimize for different block sizes
- [ ] Profile and eliminate remaining bottlenecks

### Stage 5: Deployment (Final)
- [ ] Make batch processing default for GPU
- [ ] Update documentation
- [ ] Create migration guide
- [ ] Monitor user feedback

---

## CLI Changes

Add command-line options for batch processing:

```bash
# Enable batch processing (default for GPU)
python decode.py vhs --gpu --batch-size 10 input.u8 output

# Disable batch processing (for testing)
python decode.py vhs --gpu --no-batching input.u8 output

# Auto-tune batch size based on VRAM
python decode.py vhs --gpu --auto-batch input.u8 output
```

**Implementation in decode.py:**
```python
parser.add_argument('--batch-size', type=int, default=None,
                    help='Number of blocks to process per GPU batch (default: auto)')
parser.add_argument('--no-batching', action='store_true',
                    help='Disable batch processing (process blocks individually)')
parser.add_argument('--auto-batch', action='store_true',
                    help='Automatically determine optimal batch size')
```

---

## Expected Performance

### Current (Phase 2 Complete)
- FPS: 2.49
- Transfer overhead: 20.9%
- GPU utilization: Low (idle between blocks)

### After Phase 3A (Minimal Integration)
- FPS: 3.8-4.2 (+53-69%)
- Transfer overhead: 8-10% (↓12%)
- GPU utilization: Medium (batch processing)

### After Phase 3B (Full Integration)
- FPS: 4.5-5.0 (+81-101%)
- Transfer overhead: 5-7% (↓15%)
- GPU utilization: High (optimized batching)

---

## Risks & Mitigations

### Risk 1: Block Order Issues
**Impact:** Corrupted output if blocks processed out of order  
**Mitigation:** Extensive testing with order verification

### Risk 2: Memory Leaks
**Impact:** GPU runs out of memory during long decodes  
**Mitigation:** Proper cleanup after each batch, monitoring

### Risk 3: Performance Regression
**Impact:** Batch processing slower than individual for some cases  
**Mitigation:** Fallback to individual processing, configurable batch size

### Risk 4: Compatibility Issues
**Impact:** Breaks existing workflows or tools  
**Mitigation:** Feature flag, extensive testing, gradual rollout

---

## Recommendations

### Immediate Action (Phase 3A)

1. **Implement minimal batch integration** as described above
2. **Test thoroughly** with sample captures
3. **Measure performance** gains
4. **Document** usage and limitations

**Timeline:** 3-5 days  
**Risk:** Low  
**Expected Gain:** +50-70% performance

### Future Consideration (Phase 3B)

Only pursue full integration if:
- Phase 3A successful and stable
- Performance gains justify effort
- Team has bandwidth for major refactor

**Timeline:** 1-2 weeks  
**Risk:** High  
**Expected Additional Gain:** +15-30% on top of Phase 3A

---

## Conclusion

Phase 3 integration is more complex than initially anticipated due to the DemodCache architecture. The recommended approach is:

1. **Start with Phase 3A** (minimal, low-risk integration)
2. **Validate performance and stability**
3. **Consider Phase 3B** only if needed

This approach balances performance gains with development risk and maintains backward compatibility.

---

**Document Version:** 1.0  
**Last Updated:** December 15, 2025  
**Status:** Planning Complete, Ready for Implementation  
**Next Step:** Implement Phase 3A minimal integration
