#pragma once

#include "vhsdecode/types.hpp"
#include <memory>
#include <complex>

namespace vhsdecode {
namespace rf {

/**
 * @brief FFT engine for frequency-domain processing
 * 
 * Provides FFT/iFFT operations for RF signal processing.
 * Currently CPU-only (FFTW3), GPU support (clFFT) to be added later.
 */
class FFTEngine {
public:
    /**
     * @brief Create FFT engine
     * @param blockSize FFT block size (must be power of 2)
     * @param useGPU Use GPU acceleration if available
     */
    explicit FFTEngine(size_t blockSize, bool useGPU = false);
    
    /**
     * @brief Destructor
     */
    ~FFTEngine();
    
    // Disable copy, allow move
    FFTEngine(const FFTEngine&) = delete;
    FFTEngine& operator=(const FFTEngine&) = delete;
    FFTEngine(FFTEngine&&) noexcept;
    FFTEngine& operator=(FFTEngine&&) noexcept;
    
    /**
     * @brief Get block size
     */
    size_t getBlockSize() const { return blockSize_; }
    
    /**
     * @brief Check if using GPU
     */
    bool isUsingGPU() const { return useGPU_; }
    
    /**
     * @brief Forward FFT: real to complex
     * @param input Real input array (blockSize samples)
     * @return Complex frequency-domain array (blockSize/2+1 bins)
     */
    ComplexArray forwardFFT(const RealArray& input);
    
    /**
     * @brief Inverse FFT: complex to real
     * @param input Complex frequency-domain array
     * @return Real time-domain array (blockSize samples)
     */
    RealArray inverseFFT(const ComplexArray& input);

    /**
     * @brief Inverse FFT: complex to complex
     * @param input Complex frequency-domain array (blockSize bins)
     * @return Complex time-domain array (blockSize samples)
     */
    ComplexArray complexInverseFFT(const ComplexArray& input);
    
    /**
     * @brief Apply frequency-domain filter
     * @param fftData FFT data to filter (modified in-place)
     * @param filter Frequency-domain filter coefficients
     */
    void applyFilter(ComplexArray& fftData, const ComplexArray& filter);

private:
    class Impl;  // Forward declaration for pimpl
    std::unique_ptr<Impl> impl_;
    
    size_t blockSize_;
    bool useGPU_;
};

} // namespace rf
} // namespace vhsdecode
