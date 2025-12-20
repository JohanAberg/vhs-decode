#include "vhsdecode/core/hybrid_processor.hpp"
#include "vhsdecode/rf_processor.hpp"
#include <iostream>
#include <iomanip>

namespace vhsdecode {
namespace core {

HybridProcessor::HybridProcessor(
    std::unique_ptr<rf::RFProcessor> cpuProcessor,
    std::unique_ptr<rf::RFProcessor> gpuProcessor,
    ProcessingMode mode
)
    : cpuProcessor_(std::move(cpuProcessor))
    , gpuProcessor_(std::move(gpuProcessor))
    , profiler_()
    , selector_(mode)
    , profilingEnabled_(true)
{}

HybridProcessor::~HybridProcessor() = default;

HybridProcessor::HybridProcessor(HybridProcessor&&) noexcept = default;
HybridProcessor& HybridProcessor::operator=(HybridProcessor&&) noexcept = default;

ProcessingResult HybridProcessor::processBlock(const RealArray& rfData) {
    // Analyze block
    BlockProfile profile = profiler_.analyzeBlock(
        rfData.size() * sizeof(double),
        32768,  // TODO: Get actual FFT size from config
        false   // TODO: Detect dropouts
    );
    
    // Select processor
    bool useGPU = selector_.selectGPU(profile, isGPUAvailable());
    
    // Process
    ProcessingResult result;
    if (useGPU) {
        result = processGPU(rfData);
    } else {
        result = processCPU(rfData);
    }
    
    // Record statistics
    if (profilingEnabled_) {
        profiler_.recordExecution(result.usedGPU, result.processingTimeMs);
    }
    
    return result;
}

void HybridProcessor::enableProfiling(bool enable) {
    profilingEnabled_ = enable;
}

void HybridProcessor::printStatistics() const {
    std::cout << "\n";
    std::cout << "============================================================" << std::endl;
    std::cout << "          Hybrid Processor Statistics" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Total blocks processed: " << profiler_.getTotalBlocks() << std::endl;
    std::cout << "GPU blocks: " << profiler_.getGPUBlocks() 
              << " (" << std::fixed << std::setprecision(1)
              << (100.0 * profiler_.getGPUBlocks() / profiler_.getTotalBlocks()) 
              << "%)" << std::endl;
    std::cout << "CPU blocks: " << (profiler_.getTotalBlocks() - profiler_.getGPUBlocks())
              << " (" << std::fixed << std::setprecision(1)
              << (100.0 * (profiler_.getTotalBlocks() - profiler_.getGPUBlocks()) / profiler_.getTotalBlocks())
              << "%)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Average CPU time: " << std::fixed << std::setprecision(2)
              << profiler_.getAverageCPUTime() << " ms" << std::endl;
    std::cout << "Average GPU time: " << std::fixed << std::setprecision(2)
              << profiler_.getAverageGPUTime() << " ms" << std::endl;
    std::cout << "GPU speedup: " << std::fixed << std::setprecision(1)
              << profiler_.getGPUSpeedup() << "x" << std::endl;
    std::cout << std::endl;
    
    std::cout << "============================================================" << std::endl;
}

void HybridProcessor::resetStatistics() {
    profiler_.reset();
}

ProcessingResult HybridProcessor::processCPU(const RealArray& rfData) {
    ProcessingResult result;
    result.usedGPU = false;
    
    result.processingTimeMs = measureTime([&]() {
        auto rfResult = cpuProcessor_->processBlock(rfData);
        result.analyticSignal = rfResult.analyticSignal;
        result.envelope = rfResult.envelope;
        result.filtered = rfResult.filtered;
    });
    
    return result;
}

ProcessingResult HybridProcessor::processGPU(const RealArray& rfData) {
    ProcessingResult result;
    result.usedGPU = true;
    
    try {
        result.processingTimeMs = measureTime([&]() {
            auto rfResult = gpuProcessor_->processBlock(rfData);
            result.analyticSignal = rfResult.analyticSignal;
            result.envelope = rfResult.envelope;
            result.filtered = rfResult.filtered;
        });
    } catch (const std::exception& e) {
        std::cerr << "GPU processing failed: " << e.what() << std::endl;
        std::cerr << "Falling back to CPU" << std::endl;
        
        // Fallback to CPU
        result = processCPU(rfData);
    }
    
    return result;
}

} // namespace core
} // namespace vhsdecode
