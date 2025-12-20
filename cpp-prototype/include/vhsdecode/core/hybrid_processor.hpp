#pragma once

#include "vhsdecode/core/workload_profiler.hpp"
#include "vhsdecode/core/processor_selector.hpp"
#include "vhsdecode/types.hpp"
#include <memory>
#include <chrono>

namespace vhsdecode {

// Forward declarations
namespace rf {
    class RFProcessor;
}

namespace core {

/**
 * @brief Processing result with timing information
 */
struct ProcessingResult {
    ComplexArray analyticSignal;
    RealArray envelope;
    RealArray filtered;
    double processingTimeMs;
    bool usedGPU;
    
    ProcessingResult()
        : processingTimeMs(0.0)
        , usedGPU(false)
    {}
};

/**
 * @brief Hybrid CPU/GPU processor with intelligent workload distribution
 * 
 * Automatically selects CPU or GPU for each block based on:
 * - Block characteristics (size, complexity)
 * - Historical performance data
 * - GPU availability
 * - User-specified mode
 */
class HybridProcessor {
public:
    /**
     * @brief Create hybrid processor
     * @param cpuProcessor CPU processor instance
     * @param gpuProcessor GPU processor instance (nullptr if GPU unavailable)
     * @param mode Initial processing mode
     */
    HybridProcessor(
        std::unique_ptr<rf::RFProcessor> cpuProcessor,
        std::unique_ptr<rf::RFProcessor> gpuProcessor = nullptr,
        ProcessingMode mode = ProcessingMode::Auto
    );
    
    /**
     * @brief Destructor
     */
    ~HybridProcessor();
    
    // Delete copy, allow move
    HybridProcessor(const HybridProcessor&) = delete;
    HybridProcessor& operator=(const HybridProcessor&) = delete;
    HybridProcessor(HybridProcessor&&) noexcept;
    HybridProcessor& operator=(HybridProcessor&&) noexcept;
    
    /**
     * @brief Process RF block
     * @param rfData Raw RF samples
     * @return Processing result with timing info
     * 
     * Automatically selects CPU or GPU based on profiling.
     */
    ProcessingResult processBlock(const RealArray& rfData);
    
    /**
     * @brief Enable/disable profiling
     * @param enable True to enable detailed profiling
     */
    void enableProfiling(bool enable);
    
    /**
     * @brief Check if profiling is enabled
     */
    bool isProfilingEnabled() const { return profilingEnabled_; }
    
    /**
     * @brief Get workload profiler
     */
    const WorkloadProfiler& getProfiler() const { return profiler_; }
    
    /**
     * @brief Get processor selector
     */
    ProcessorSelector& getSelector() { return selector_; }
    const ProcessorSelector& getSelector() const { return selector_; }
    
    /**
     * @brief Check if GPU is available
     */
    bool isGPUAvailable() const { return gpuProcessor_ != nullptr; }
    
    /**
     * @brief Print performance statistics
     */
    void printStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    std::unique_ptr<rf::RFProcessor> cpuProcessor_;
    std::unique_ptr<rf::RFProcessor> gpuProcessor_;
    
    WorkloadProfiler profiler_;
    ProcessorSelector selector_;
    
    bool profilingEnabled_;
    
    /**
     * @brief Process block on CPU
     * @param rfData Raw RF samples
     * @return Processing result
     */
    ProcessingResult processCPU(const RealArray& rfData);
    
    /**
     * @brief Process block on GPU
     * @param rfData Raw RF samples
     * @return Processing result
     * 
     * Falls back to CPU on error.
     */
    ProcessingResult processGPU(const RealArray& rfData);
    
    /**
     * @brief Measure execution time
     */
    template<typename Func>
    double measureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

} // namespace core
} // namespace vhsdecode
