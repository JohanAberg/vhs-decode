import threading
import time
import logging

# Use lddecode logger to ensure output is captured by the main logging setup
logger = logging.getLogger("lddecode")

class AsyncLoader:
    def __init__(self, loader_func, infile, blocksize, blocklen, queue_size=32):
        self.loader_func = loader_func
        self.infile = infile
        self.blocksize = blocksize
        self.blocklen = blocklen
        self.queue_size = queue_size
        
        self.cache = {} # block_index -> data
        self.lock = threading.Lock()
        self.file_lock = threading.Lock()
        self.condition = threading.Condition(self.lock)
        
        self.next_block = 0
        self.running = True
        self.thread = threading.Thread(target=self._worker, daemon=True, name="AsyncLoader")
        self.thread.start()
        
        logger.info(f"AsyncLoader initialized (queue_size={queue_size})")

    def _worker(self):
        while self.running:
            with self.lock:
                # Determine what to read
                current_cache_size = len(self.cache)
                target_block = self.next_block
                
            if current_cache_size < self.queue_size:
                # Read next block
                offset = target_block * self.blocksize
                
                try:
                    with self.file_lock:
                        data = self.loader_func(self.infile, offset, self.blocklen)
                except Exception as e:
                    logger.error(f"AsyncLoader read error: {e}")
                    data = None
                
                with self.lock:
                    # Only store if we haven't been reset/seeked away
                    if self.next_block == target_block:
                        self.cache[target_block] = data
                        self.next_block += 1
                        self.condition.notify_all()
                    else:
                        # We were reset, discard this data
                        pass
            else:
                # Cache full, wait
                with self.lock:
                    self.condition.wait(0.1)

    def __call__(self, infile, offset, length):
        return self.read(infile, offset, length)

    def read(self, infile, offset, length):
        # We ignore the passed infile and use self.infile to ensure thread safety via file_lock
        # We assume length == self.blocklen
        
        block_idx = offset // self.blocksize
        
        with self.lock:
            # Check if in cache
            if block_idx in self.cache:
                data = self.cache.pop(block_idx)
                self.condition.notify() # Notify worker that space is available
                return data
            
            # Cache miss.
            # If block_idx is not what worker is working on, reset worker.
            if block_idx != self.next_block:
                # Seek detected
                self.next_block = block_idx
                self.cache.clear()
                self.condition.notify() # Wake worker
            
            # Wait for worker to produce the block
            while block_idx not in self.cache and self.running:
                self.condition.wait()
                
            if block_idx in self.cache:
                data = self.cache.pop(block_idx)
                self.condition.notify()
                return data
            
            # If we are here, it means we stopped running or something failed
            # Try a direct read as last resort if we are still running but cache failed
            if self.running:
                with self.file_lock:
                    return self.loader_func(self.infile, offset, length)
                
            return None

    def _close(self):
        self.running = False
        with self.lock:
            self.condition.notify_all()
        self.thread.join(timeout=1.0)
        
        if hasattr(self.loader_func, "_close") and callable(self.loader_func._close):
            self.loader_func._close()
