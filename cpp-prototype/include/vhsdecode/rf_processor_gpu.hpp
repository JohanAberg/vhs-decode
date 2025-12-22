#pragma once

#if defined(HAVE_OPENCL) && defined(HAVE_CLFFT)

#include "vhsdecode/rf_processor.hpp"
#include "vhsdecode/opencl_context.hpp"
#include "vhsdecode/clfft_engine.hpp"
#include "vhsdecode/gpu/envelope_kernel.hpp"
#include "vhsdecode/gpu/phase_unwrap_kernel.hpp"
#include <memory>

namespace vhsdecode {
namespace rf {

/**
 * @brief GPU-accelerated RF processor
 * 
 * Implements the RF processing pipeline on GPU using OpenCL and clFFT.
 * Falls back to CPU if GPU operations fail.
 */
class RFProcessorGPU {
public:
    using Config = RFProcessor::Config;
    using Result = RFProcessor::Result;
    
    /**
     * @brief Construct GPU RF processor
     * @param config Processing configuration
     * @throws std::runtime_error if GPU initialization fails
     */
    explicit RFProcessorGPU(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~RFProcessorGPU();
    
    // Non-copyable, movable
    RFProcessorGPU(const RFProcessorGPU&) = delete;
    RFProcessorGPU& operator=(const RFProcessorGPU&) = delete;
    RFProcessorGPU(RFProcessorGPU&&) noexcept;
    RFProcessorGPU& operator=(RFProcessorGPU&&) noexcept;
    
    /**
     * @brief Process RF block on GPU
     * @param rfData Input RF samples (normalized float)
     * @return Processing result with analytic signal, envelope, filtered signal
     */
    Result processBlock(const RealArray& rfData);
    
    /**
     * @brief Process RF block from uint8 samples on GPU
     * @param rfSamples Raw RF samples (uint8)
     * @return Processing result
     */
    Result processBlock(const std::vector<uint8_t>& rfSamples);
    
    /**
     * @brief Get configuration
     */
    const Config& getConfig() const { return config_; }
    
    /**
     * @brief Check if GPU is being used
     */
    bool isUsingGPU() const { return usingGPU_; }
    
    /**
     * @brief Get filter bank (returns CPU fallback filter bank)
     */
    FilterBank& getFilterBank() {
        if (!cpuFallback_) {
            Config cpuConfig = config_;
            cpuConfig.useGPU = false;
            cpuFallback_ = std::make_unique<RFProcessor>(cpuConfig);
        }
        return cpuFallback_->getFilterBank();
    }
    
private:
    Config config_;
    bool usingGPU_;
    
    // GPU components
    std::unique_ptr<gpu::OpenCLContext> clContext_;
    std::unique_ptr<gpu::CLFFTEngine> clfftEngine_;
    std::unique_ptr<gpu::EnvelopeKernel> envelopeKernel_;
    
    // GPU buffers for reuse
    cl::Buffer inputBuffer_;
    cl::Buffer outputBuffer_;
    cl::Buffer analyticBuffer_;
    cl::Buffer envelopeBuffer_;
    
    // CPU fallback
    std::unique_ptr<RFProcessor> cpuFallback_;
    
    // Helper methods
    RealArray uint8ToFloat(const std::vector<uint8_t>& data);
    void initializeGPU();
    Result processBlockGPU(const RealArray& rfData);
    Result processBlockCPU(const RealArray& rfData);
};

} // namespace rf
} // namespace vhsdecode

#endif // defined(HAVE_OPENCL) && defined(HAVE_CLFFT)
