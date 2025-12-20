#pragma once

#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include <vector>

namespace vhsdecode {
namespace gpu {

/**
 * @brief GPU kernel wrapper for phase unwrapping
 * 
 * Provides high-level interface to OpenCL phase unwrap kernels.
 * Used in FM demodulation pipeline.
 */
class PhaseUnwrapKernel {
public:
    /**
     * @brief Create phase unwrap kernel
     * @param ctx OpenCL context
     */
    explicit PhaseUnwrapKernel(OpenCLContext& ctx);
    
    /**
     * @brief Destructor
     */
    ~PhaseUnwrapKernel() = default;
    
    // Delete copy, allow move
    PhaseUnwrapKernel(const PhaseUnwrapKernel&) = delete;
    PhaseUnwrapKernel& operator=(const PhaseUnwrapKernel&) = delete;
    PhaseUnwrapKernel(PhaseUnwrapKernel&&) noexcept = default;
    PhaseUnwrapKernel& operator=(PhaseUnwrapKernel&&) noexcept = default;
    
    /**
     * @brief Unwrap phase (CPU interface)
     * @param phaseIn Input phase values
     * @return Unwrapped phase values
     * 
     * This performs GPU unwrapping with host↔device transfers.
     */
    std::vector<double> unwrap(const std::vector<double>& phaseIn);
    
    /**
     * @brief Unwrap phase (GPU interface)
     * @param phaseInBuffer Input phase buffer on GPU
     * @param phaseOutBuffer Output phase buffer on GPU
     * @param N Number of samples
     * 
     * Operates directly on GPU buffers, avoiding host↔device transfers.
     */
    void unwrapGPU(cl::Buffer& phaseInBuffer, cl::Buffer& phaseOutBuffer, size_t N);
    
    /**
     * @brief Compute phase differences
     * @param phaseIn Input phase values
     * @param phaseDiff Output phase differences
     * @param N Number of samples
     * 
     * First step of two-stage unwrapping for better parallelization.
     */
    void computePhaseDiff(cl::Buffer& phaseIn, cl::Buffer& phaseDiff, size_t N);
    
    /**
     * @brief Accumulate phase differences
     * @param phaseDiff Input phase differences
     * @param phaseOut Output unwrapped phase
     * @param N Number of samples
     * 
     * Second step of two-stage unwrapping.
     */
    void accumulatePhaseDiff(cl::Buffer& phaseDiff, cl::Buffer& phaseOut, size_t N);

private:
    OpenCLContext& ctx_;
    cl::Program program_;
    cl::Kernel unwrapSimpleKernel_;
    cl::Kernel phaseDiffKernel_;
    cl::Kernel phaseAccumulateKernel_;
    
    // Temporary buffers for internal use
    cl::Buffer tempInBuffer_;
    cl::Buffer tempOutBuffer_;
    cl::Buffer tempDiffBuffer_;
    size_t bufferSize_;
    
    /**
     * @brief Load and compile OpenCL kernels
     */
    void loadKernels();
    
    /**
     * @brief Allocate temporary buffers
     * @param size Buffer size in number of doubles
     */
    void allocateBuffers(size_t size);
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
