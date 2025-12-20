#pragma once

#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include <vector>
#include <complex>

namespace vhsdecode {
namespace gpu {

/**
 * @brief GPU kernel wrapper for filter application
 * 
 * Provides high-level interface to OpenCL filter kernels.
 * Applies frequency-domain filters to FFT data.
 */
class FilterKernel {
public:
    /**
     * @brief Create filter kernel
     * @param ctx OpenCL context
     */
    explicit FilterKernel(OpenCLContext& ctx);
    
    /**
     * @brief Destructor
     */
    ~FilterKernel() = default;
    
    // Delete copy, allow move
    FilterKernel(const FilterKernel&) = delete;
    FilterKernel& operator=(const FilterKernel&) = delete;
    FilterKernel(FilterKernel&&) noexcept = default;
    FilterKernel& operator=(FilterKernel&&) noexcept = default;
    
    /**
     * @brief Apply frequency-domain filter (CPU interface)
     * @param fftData Complex FFT data
     * @param filterResponse Real filter coefficients
     * @return Filtered FFT data
     * 
     * This performs GPU filtering with host↔device transfers.
     */
    std::vector<std::complex<double>> applyFilter(
        const std::vector<std::complex<double>>& fftData,
        const std::vector<double>& filterResponse
    );
    
    /**
     * @brief Apply frequency-domain filter (GPU interface)
     * @param fftDataBuffer Complex FFT data buffer on GPU (in/out)
     * @param filterBuffer Real filter response buffer on GPU
     * @param N Number of frequency bins
     * 
     * Operates directly on GPU buffers, avoiding host↔device transfers.
     * This is the most efficient interface for pipeline processing.
     */
    void applyFilterGPU(
        cl::Buffer& fftDataBuffer,
        cl::Buffer& filterBuffer,
        size_t N
    );
    
    /**
     * @brief Apply complex frequency-domain filter
     * @param fftDataBuffer Complex FFT data buffer on GPU (in/out)
     * @param filterBuffer Complex filter response buffer on GPU
     * @param N Number of frequency bins
     */
    void applyComplexFilterGPU(
        cl::Buffer& fftDataBuffer,
        cl::Buffer& filterBuffer,
        size_t N
    );
    
    /**
     * @brief Apply multiple filters in sequence
     * @param fftDataBuffer Complex FFT data buffer on GPU (in/out)
     * @param filtersBuffer Array of filter responses on GPU
     * @param numFilters Number of filters
     * @param N Number of frequency bins
     */
    void applyMultipleFiltersGPU(
        cl::Buffer& fftDataBuffer,
        cl::Buffer& filtersBuffer,
        size_t numFilters,
        size_t N
    );
    
    /**
     * @brief Apply bandpass filter
     * @param fftDataBuffer Complex FFT data buffer on GPU (in/out)
     * @param N Number of frequency bins
     * @param lowCutoff Low cutoff frequency (normalized 0-1)
     * @param highCutoff High cutoff frequency (normalized 0-1)
     * @param rolloff Transition rolloff factor
     */
    void applyBandpassFilterGPU(
        cl::Buffer& fftDataBuffer,
        size_t N,
        double lowCutoff,
        double highCutoff,
        double rolloff = 0.05
    );
    
    /**
     * @brief Normalize data
     * @param dataBuffer Real data buffer on GPU (in/out)
     * @param N Number of samples
     * @param scale Normalization scale factor
     */
    void normalizeDataGPU(cl::Buffer& dataBuffer, size_t N, double scale);
    
    /**
     * @brief Normalize complex data
     * @param dataBuffer Complex data buffer on GPU (in/out)
     * @param N Number of samples
     * @param scale Normalization scale factor
     */
    void normalizeComplexDataGPU(cl::Buffer& dataBuffer, size_t N, double scale);

private:
    OpenCLContext& ctx_;
    cl::Program program_;
    cl::Kernel applyFilterKernel_;
    cl::Kernel applyComplexFilterKernel_;
    cl::Kernel applyMultipleFiltersKernel_;
    cl::Kernel applyBandpassKernel_;
    cl::Kernel normalizeKernel_;
    cl::Kernel normalizeComplexKernel_;
    
    // Temporary buffers for internal use
    cl::Buffer tempFFTBuffer_;
    cl::Buffer tempFilterBuffer_;
    size_t bufferSize_;
    
    /**
     * @brief Load and compile OpenCL kernels
     */
    void loadKernels();
    
    /**
     * @brief Allocate temporary buffers
     * @param size Buffer size in number of complex samples
     */
    void allocateBuffers(size_t size);
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
