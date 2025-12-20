#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace vhsdecode {
namespace core {

/**
 * @brief Generic RF block for processing
 */
struct RFBlock {
    std::vector<uint8_t> data;
    size_t blockNumber;
    size_t offset;
};

/**
 * @brief Result of processing an RF block
 */
struct BlockResult {
    std::vector<double> video;
    std::vector<double> chroma;
    size_t blockNumber;
    bool success;
};

/**
 * @brief Multi-threaded block processor using thread pool pattern
 * 
 * Processes RF blocks in parallel across multiple threads with
 * lock-free work queue and thread-safe result collection.
 */
class BlockProcessor {
public:
    using ProcessorFunction = std::function<BlockResult(const RFBlock&)>;

    /**
     * @brief Construct block processor with thread pool
     * @param numThreads Number of worker threads (1-16)
     * @param processor Function to process each block
     */
    BlockProcessor(size_t numThreads, ProcessorFunction processor);

    /**
     * @brief Destructor - gracefully shuts down threads
     */
    ~BlockProcessor();

    /**
     * @brief Submit RF block for processing
     * @param block Block to process
     */
    void submitBlock(const RFBlock& block);

    /**
     * @brief Get next processed result (blocks if none available)
     * @return Processed block result
     */
    BlockResult getResult();

    /**
     * @brief Check if results are available
     * @return True if results available
     */
    bool hasResults() const;

    /**
     * @brief Get number of blocks in work queue
     */
    size_t getQueueSize() const;

private:
    std::vector<std::thread> workers_;
    std::queue<RFBlock> workQueue_;
    std::queue<BlockResult> resultQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable workCV_;
    std::condition_variable resultCV_;
    std::atomic<bool> running_;
    ProcessorFunction processor_;

    void workerThread();
};

} // namespace core
} // namespace vhsdecode
