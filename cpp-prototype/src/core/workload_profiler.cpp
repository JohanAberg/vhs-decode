#include "vhsdecode/core/workload_profiler.hpp"
#include <algorithm>
#include <cmath>

namespace vhsdecode {
namespace core {

WorkloadProfiler::WorkloadProfiler()
    : totalCPUTime_(0.0)
    , totalGPUTime_(0.0)
    , cpuSamples_(0)
    , gpuSamples_(0)
    , totalBlocks_(0)
    , gpuBlocks_(0)
{}

BlockProfile WorkloadProfiler::analyzeBlock(size_t dataSize, size_t fftSize, bool hasDropouts) {
    BlockProfile profile;
    profile.dataSize = dataSize;
    profile.fftSize = fftSize;
    profile.hasDropouts = hasDropouts;
    profile.complexity = calculateComplexity(dataSize, fftSize, hasDropouts);
    
    totalBlocks_++;
    
    return profile;
}

void WorkloadProfiler::recordExecution(bool usedGPU, double executionTimeMs) {
    if (usedGPU) {
        totalGPUTime_ += executionTimeMs;
        gpuSamples_++;
        gpuBlocks_++;
    } else {
        totalCPUTime_ += executionTimeMs;
        cpuSamples_++;
    }
}

double WorkloadProfiler::getAverageCPUTime() const {
    if (cpuSamples_ == 0) return 0.0;
    return totalCPUTime_ / cpuSamples_;
}

double WorkloadProfiler::getAverageGPUTime() const {
    if (gpuSamples_ == 0) return 0.0;
    return totalGPUTime_ / gpuSamples_;
}

double WorkloadProfiler::getGPUSpeedup() const {
    double cpuAvg = getAverageCPUTime();
    double gpuAvg = getAverageGPUTime();
    
    if (gpuAvg == 0.0 || cpuAvg == 0.0) return 0.0;
    return cpuAvg / gpuAvg;
}

void WorkloadProfiler::reset() {
    totalCPUTime_ = 0.0;
    totalGPUTime_ = 0.0;
    cpuSamples_ = 0;
    gpuSamples_ = 0;
    totalBlocks_ = 0;
    gpuBlocks_ = 0;
}

float WorkloadProfiler::calculateComplexity(size_t dataSize, size_t fftSize, bool hasDropouts) {
    // Base complexity from data size
    float complexity = 0.3f;
    
    // FFT adds significant complexity
    if (fftSize > 0) {
        // Larger FFTs are more complex
        // log2(fftSize) normalized to 0-1 range
        // Typical range: 1024 (10) to 65536 (16)
        float fftComplexity = std::log2(static_cast<float>(fftSize));
        fftComplexity = (fftComplexity - 10.0f) / 6.0f;  // Normalize to 0-1
        fftComplexity = std::max(0.0f, std::min(1.0f, fftComplexity));
        complexity += fftComplexity * 0.5f;
    }
    
    // Dropouts reduce complexity (less data to process)
    if (hasDropouts) {
        complexity *= 0.8f;
    }
    
    // Clamp to 0-1 range
    return std::max(0.0f, std::min(1.0f, complexity));
}

} // namespace core
} // namespace vhsdecode
