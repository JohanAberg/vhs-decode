#pragma once

#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include <vector>
#include <complex>

namespace vhsdecode {
namespace gpu {

/**
 * @brief GPU kernel wrapper for envelope detection
 * 
 * Provides high-level interface to OpenCL envelope detection kernels.
 * Used for dropout detection and signal quality assessment.
 */
class EnvelopeKernel {
public:
    /**
     * @brief Create envelope detection kernel
     * @param ctx OpenCL context
     */
    explicit EnvelopeKernel(OpenCLContext& ctx);
    
    /**
     * @brief Destructor
     */
    ~EnvelopeKernel() = default;
    
    // Delete copy, allow move
    EnvelopeKernel(const EnvelopeKernel&) = delete;
    EnvelopeKernel& operator=(const EnvelopeKernel&) = delete;
    EnvelopeKernel(EnvelopeKernel&&) noexcept = default;
    EnvelopeKernel& operator=(EnvelopeKernel&&) noexcept = default;
    
    /**
     * @brief Detect envelope (CPU interface)
     * @param analyticSignal Complex analytic signal
     * @return Envelope (magnitude) values
     * 
     * This performs GPU envelope detection with host↔device transfers.
     */
    std::vector<double> detect(const std::vector<std::complex<double>>& analyticSignal);
    
    /**
     * @brief Detect envelope (GPU interface)
     * @param analyticBuffer Complex analytic signal buffer on GPU
     * @param envelopeBuffer Envelope output buffer on GPU
     * @param N Number of samples
     * 
     * Operates directly on GPU buffers, avoiding host↔device transfers.
     */
    void detectGPU(cl::Buffer& analyticBuffer, cl::Buffer& envelopeBuffer, size_t N);
    
    /**
     * @brief Detect envelope with filtering
     * @param analyticBuffer Complex analytic signal buffer on GPU
     * @param envelopeBuffer Smoothed envelope output buffer on GPU
     * @param N Number of samples
     * @param windowSize Moving average window size
     */
    void detectFilteredGPU(
        cl::Buffer& analyticBuffer,
        cl::Buffer& envelopeBuffer,
        size_t N,
        int windowSize
    );
    
    /**
     * @brief Compute instantaneous frequency
     * @param analyticBuffer Complex analytic signal buffer on GPU
     * @param frequencyBuffer Frequency output buffer on GPU
     * @param N Number of samples
     * @param sampleRate Sample rate in Hz
     */
    void computeInstantaneousFrequency(
        cl::Buffer& analyticBuffer,
        cl::Buffer& frequencyBuffer,
        size_t N,
        double sampleRate
    );

private:
    OpenCLContext& ctx_;
    cl::Program program_;
    cl::Kernel detectKernel_;
    cl::Kernel detectFastKernel_;
    cl::Kernel detectFilteredKernel_;
    cl::Kernel instantaneousFreqKernel_;
    
    // Temporary buffers for internal use
    cl::Buffer tempAnalyticBuffer_;
    cl::Buffer tempEnvelopeBuffer_;
    size_t bufferSize_;
    
    /**
     * @brief Load and compile OpenCL kernels
     */
    void loadKernels();
    
    /**
     * @brief Allocate temporary buffers
     * @param size Buffer size in number of samples
     */
    void allocateBuffers(size_t size);
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
