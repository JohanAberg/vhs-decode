#pragma once

#include "vhsdecode/core/workload_profiler.hpp"

namespace vhsdecode {
namespace core {

/**
 * @brief Processing mode selection
 */
enum class ProcessingMode {
    CPU,   // Force CPU processing
    GPU,   // Force GPU processing
    Auto   // Automatic selection based on profiling
};

/**
 * @brief Selects optimal processor (CPU/GPU) for each block
 * 
 * Uses workload profiling to intelligently distribute work between CPU and GPU.
 */
class ProcessorSelector {
public:
    /**
     * @brief Create processor selector
     * @param mode Initial processing mode
     */
    explicit ProcessorSelector(ProcessingMode mode = ProcessingMode::Auto);
    
    /**
     * @brief Select processor for a block
     * @param profile Block profile from WorkloadProfiler
     * @param gpuAvailable True if GPU is available
     * @return True to use GPU, false to use CPU
     */
    bool selectGPU(const BlockProfile& profile, bool gpuAvailable);
    
    /**
     * @brief Set processing mode
     * @param mode New processing mode
     */
    void setMode(ProcessingMode mode);
    
    /**
     * @brief Get current processing mode
     */
    ProcessingMode getMode() const { return mode_; }
    
    /**
     * @brief Set CPU complexity threshold
     * @param threshold Blocks with complexity < threshold use CPU (0.0-1.0)
     * 
     * Lower threshold means more blocks go to GPU.
     * Higher threshold means more blocks go to CPU.
     */
    void setCPUThreshold(float threshold);
    
    /**
     * @brief Get CPU threshold
     */
    float getCPUThreshold() const { return cpuThreshold_; }
    
    /**
     * @brief Set minimum FFT size for GPU
     * @param size Minimum FFT size (blocks smaller than this use CPU)
     * 
     * Small FFTs have too much overhead for GPU processing.
     */
    void setMinGPUFFTSize(size_t size);
    
    /**
     * @brief Get minimum GPU FFT size
     */
    size_t getMinGPUFFTSize() const { return minGPUFFTSize_; }

private:
    ProcessingMode mode_;
    float cpuThreshold_;        // Complexity threshold (0.0-1.0)
    size_t minGPUFFTSize_;      // Minimum FFT size for GPU
    
    /**
     * @brief Automatic selection based on profile
     * @param profile Block profile
     * @return True to use GPU
     */
    bool autoSelect(const BlockProfile& profile);
};

} // namespace core
} // namespace vhsdecode
