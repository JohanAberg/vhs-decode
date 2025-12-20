#include "vhsdecode/block_processor.hpp"
#include <iostream>

namespace vhsdecode {
namespace core {

BlockProcessor::BlockProcessor(size_t numThreads, ProcessorFunction processor)
    : running_(true)
    , processor_(std::move(processor))
{
    // Limit thread count
    numThreads = std::min(numThreads, size_t(16));
    numThreads = std::max(numThreads, size_t(1));

    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this]() { workerThread(); });
    }

    std::cout << "✓ Block processor initialized" << std::endl;
    std::cout << "  Thread count: " << numThreads << std::endl;
}

BlockProcessor::~BlockProcessor() {
    // Signal threads to stop
    running_ = false;
    workCV_.notify_all();

    // Wait for all threads to finish
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void BlockProcessor::submitBlock(const RFBlock& block) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        workQueue_.push(block);
    }
    workCV_.notify_one();
}

BlockResult BlockProcessor::getResult() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    resultCV_.wait(lock, [this]() { return !resultQueue_.empty(); });

    auto result = resultQueue_.front();
    resultQueue_.pop();
    return result;
}

bool BlockProcessor::hasResults() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !resultQueue_.empty();
}

size_t BlockProcessor::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return workQueue_.size();
}

void BlockProcessor::workerThread() {
    while (running_) {
        RFBlock block;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            workCV_.wait(lock, [this]() {
                return !workQueue_.empty() || !running_;
            });

            if (!running_) break;

            if (workQueue_.empty()) continue;

            block = workQueue_.front();
            workQueue_.pop();
        }

        // Process block (outside lock)
        BlockResult result;
        try {
            result = processor_(block);
        } catch (const std::exception& e) {
            std::cerr << "Error processing block " << block.blockNumber 
                      << ": " << e.what() << std::endl;
            result.blockNumber = block.blockNumber;
            result.success = false;
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            resultQueue_.push(result);
        }
        resultCV_.notify_one();
    }
}

} // namespace core
} // namespace vhsdecode
