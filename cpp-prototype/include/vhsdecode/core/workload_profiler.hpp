#pragma once

#include <cstddef>

namespace vhsdecode {
namespace core {

/**
 * @brief Profile of a processing block for workload analysis
 */
struct BlockProfile {
    size_t dataSize;        // Size of data in bytes
    size_t fftSize;         // FFT size (0 if no FFT needed)
    bool hasDropouts;       // True if dropouts detected
    float complexity;       // Complexity score 0.0-1.0
    
    BlockProfile()
        : dataSize(0)
        , fftSize(0)
        , hasDropouts(false)
        , complexity(0.5f)
    {}
};

/**
 * @brief Profiles processing blocks to determine workload characteristics
 * 
 * Collects statistics about block processing to optimize CPU/GPU selection.
 */
class WorkloadProfiler {
public:
    /**
     * @brief Create workload profiler
     */
    WorkloadProfiler();
    
    /**
     * @brief Analyze a processing block
     * @param dataSize Size of RF data in bytes
     * @param fftSize FFT size (0 if no FFT)
     * @param hasDropouts True if dropouts detected
     * @return Block profile with complexity score
     */
    BlockProfile analyzeBlock(size_t dataSize, size_t fftSize, bool hasDropouts);
    
    /**
     * @brief Record execution time for a processing mode
     * @param usedGPU True if GPU was used
     * @param executionTimeMs Execution time in milliseconds
     */
    void recordExecution(bool usedGPU, double executionTimeMs);
    
    /**
     * @brief Get average CPU execution time
     * @return Average time in milliseconds
     */
    double getAverageCPUTime() const;
    
    /**
     * @brief Get average GPU execution time
     * @return Average time in milliseconds
     */
    double getAverageGPUTime() const;
    
    /**
     * @brief Get GPU speedup factor
     * @return Speedup (CPU time / GPU time)
     */
    double getGPUSpeedup() const;
    
    /**
     * @brief Get total blocks processed
     */
    size_t getTotalBlocks() const { return totalBlocks_; }
    
    /**
     * @brief Get GPU blocks processed
     */
    size_t getGPUBlocks() const { return gpuBlocks_; }
    
    /**
     * @brief Reset statistics
     */
    void reset();

private:
    // Execution time statistics
    double totalCPUTime_;
    double totalGPUTime_;
    size_t cpuSamples_;
    size_t gpuSamples_;
    
    // Block statistics
    size_t totalBlocks_;
    size_t gpuBlocks_;
    
    /**
     * @brief Calculate complexity score for a block
     * @param dataSize Size of data
     * @param fftSize FFT size
     * @param hasDropouts True if dropouts present
     * @return Complexity score 0.0-1.0
     */
    float calculateComplexity(size_t dataSize, size_t fftSize, bool hasDropouts);
};

} // namespace core
} // namespace vhsdecode
