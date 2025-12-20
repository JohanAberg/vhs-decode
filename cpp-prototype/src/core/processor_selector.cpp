#include "vhsdecode/core/processor_selector.hpp"

namespace vhsdecode {
namespace core {

ProcessorSelector::ProcessorSelector(ProcessingMode mode)
    : mode_(mode)
    , cpuThreshold_(0.3f)
    , minGPUFFTSize_(4096)
{}

bool ProcessorSelector::selectGPU(const BlockProfile& profile, bool gpuAvailable) {
    // If GPU not available, use CPU
    if (!gpuAvailable) {
        return false;
    }
    
    // If mode is forced, use that
    if (mode_ == ProcessingMode::GPU) {
        return true;
    }
    if (mode_ == ProcessingMode::CPU) {
        return false;
    }
    
    // Auto mode: use profiling
    return autoSelect(profile);
}

void ProcessorSelector::setMode(ProcessingMode mode) {
    mode_ = mode;
}

void ProcessorSelector::setCPUThreshold(float threshold) {
    if (threshold < 0.0f) threshold = 0.0f;
    if (threshold > 1.0f) threshold = 1.0f;
    cpuThreshold_ = threshold;
}

void ProcessorSelector::setMinGPUFFTSize(size_t size) {
    minGPUFFTSize_ = size;
}

bool ProcessorSelector::autoSelect(const BlockProfile& profile) {
    // Small FFTs are not worth GPU overhead
    if (profile.fftSize > 0 && profile.fftSize < minGPUFFTSize_) {
        return false;
    }
    
    // Use complexity threshold
    // High complexity blocks benefit more from GPU
    if (profile.complexity >= cpuThreshold_) {
        return true;
    }
    
    // Low complexity blocks use CPU
    return false;
}

} // namespace core
} // namespace vhsdecode
