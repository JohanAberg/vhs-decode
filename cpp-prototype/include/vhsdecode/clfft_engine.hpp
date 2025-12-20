#pragma once

#ifdef HAVE_CLFFT

#include "vhsdecode/opencl_context.hpp"
#include <clFFT.h>
#include <complex>
#include <vector>
#include <memory>

namespace vhsdecode {
namespace gpu {

/**
 * @brief GPU FFT engine using clFFT library
 * 
 * Provides CPU-compatible interface while using GPU acceleration internally.
 * Matches FFTWEngine API for drop-in replacement.
 */
class CLFFTEngine {
public:
    /**
     * @brief Create GPU FFT engine
     * @param ctx OpenCL context
     * @param blockSize FFT size (number of samples)
     * @param useGPU Enable GPU acceleration (default: true)
     */
    CLFFTEngine(OpenCLContext& ctx, size_t blockSize, bool useGPU = true);
    
    /**
     * @brief Destructor - cleanup clFFT plans
     */
    ~CLFFTEngine();
    
    // Delete copy and move
    CLFFTEngine(const CLFFTEngine&) = delete;
    CLFFTEngine& operator=(const CLFFTEngine&) = delete;
    CLFFTEngine(CLFFTEngine&&) = delete;
    CLFFTEngine& operator=(CLFFTEngine&&) = delete;
    
    /**
     * @brief Forward FFT (CPU-compatible interface)
     * @param input Real input samples
     * @return Complex FFT coefficients
     * 
     * This performs GPU FFT with host↔device transfers.
     * Use forwardFFTGPU() to avoid transfers when data already on GPU.
     */
    std::vector<std::complex<double>> forwardFFT(const std::vector<double>& input);
    
    /**
     * @brief Inverse FFT (CPU-compatible interface)
     * @param input Complex FFT coefficients
     * @return Real output samples
     * 
     * This performs GPU iFFT with host↔device transfers.
     * Use inverseFFTGPU() to avoid transfers when data already on GPU.
     */
    std::vector<double> inverseFFT(const std::vector<std::complex<double>>& input);
    
    /**
     * @brief Forward FFT (GPU-optimized interface)
     * @param inputBuffer Real input buffer on GPU (blockSize samples)
     * @param outputBuffer Complex output buffer on GPU (blockSize/2+1 complex values)
     * @param keepOnDevice If true, result stays on GPU; if false, transferred to host
     * 
     * Operates directly on GPU buffers, avoiding host↔device transfers.
     */
    void forwardFFTGPU(
        cl::Buffer& inputBuffer,
        cl::Buffer& outputBuffer,
        bool keepOnDevice = true
    );
    
    /**
     * @brief Inverse FFT (GPU-optimized interface)
     * @param inputBuffer Complex input buffer on GPU (blockSize/2+1 complex values)
     * @param outputBuffer Real output buffer on GPU (blockSize samples)
     * @param keepOnDevice If true, result stays on GPU; if false, transferred to host
     */
    void inverseFFTGPU(
        cl::Buffer& inputBuffer,
        cl::Buffer& outputBuffer,
        bool keepOnDevice = true
    );
    
    /**
     * @brief Apply frequency-domain filter on GPU
     * @param fftData Complex FFT data (in/out)
     * @param filter Real filter coefficients
     * @param filterSize Number of filter coefficients
     * 
     * Multiplies fftData by filter coefficients in-place.
     * Operates entirely on GPU, no host transfers.
     */
    void applyFilterGPU(
        cl::Buffer& fftData,
        cl::Buffer& filter,
        size_t filterSize
    );
    
    /**
     * @brief Set batch size for processing multiple FFTs at once
     * @param batchSize Number of FFTs to process together
     * 
     * Batch processing improves GPU utilization.
     */
    void setBatchSize(size_t batchSize);
    
    /**
     * @brief Get FFT block size
     */
    size_t getBlockSize() const { return blockSize_; }
    
    /**
     * @brief Check if GPU acceleration is enabled
     */
    bool isGPUEnabled() const { return useGPU_; }
    
    /**
     * @brief Get current batch size
     */
    size_t getBatchSize() const { return batchSize_; }

private:
    OpenCLContext& ctx_;
    size_t blockSize_;
    size_t batchSize_;
    bool useGPU_;
    
    clfftPlanHandle forwardPlan_;
    clfftPlanHandle inversePlan_;
    
    // Temporary buffers for internal use
    cl::Buffer tempRealBuffer_;
    cl::Buffer tempComplexBuffer_;
    
    // Filter application kernel
    cl::Kernel filterKernel_;
    cl::Program filterProgram_;
    
    /**
     * @brief Initialize clFFT library (called once globally)
     */
    static void initializeClFFT();
    
    /**
     * @brief Cleanup clFFT library (called once globally)
     */
    static void cleanupClFFT();
    
    /**
     * @brief Create forward FFT plan
     */
    void createForwardPlan();
    
    /**
     * @brief Create inverse FFT plan
     */
    void createInversePlan();
    
    /**
     * @brief Initialize filter application kernel
     */
    void initializeFilterKernel();
    
    // Track if clFFT library is initialized
    static bool clFFTInitialized_;
    static int instanceCount_;
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_CLFFT
