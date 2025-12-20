#pragma once

#include "vhsdecode/types.hpp"
#include <memory>

namespace vhsdecode {
namespace rf {

/**
 * @brief Hilbert transform for generating analytic signals
 * 
 * Converts a real RF signal to a complex analytic signal using the Hilbert transform.
 * This is done efficiently in the frequency domain by zeroing negative frequencies.
 */
class HilbertTransform {
public:
    /**
     * @brief Create Hilbert transform
     * @param blockSize FFT block size
     */
    explicit HilbertTransform(size_t blockSize);
    
    /**
     * @brief Destructor
     */
    ~HilbertTransform();
    
    // Disable copy, allow move
    HilbertTransform(const HilbertTransform&) = delete;
    HilbertTransform& operator=(const HilbertTransform&) = delete;
    HilbertTransform(HilbertTransform&&) noexcept;
    HilbertTransform& operator=(HilbertTransform&&) noexcept;
    
    /**
     * @brief Apply Hilbert transform to frequency-domain data
     * @param fftData FFT of real signal (N/2+1 complex bins)
     * @return Hilbert transform filter to apply
     * 
     * The Hilbert transform in frequency domain:
     * - DC component (bin 0): multiply by 1
     * - Positive frequencies (bins 1 to N/2-1): multiply by 2
     * - Nyquist frequency (bin N/2): multiply by 1
     */
    ComplexArray getHilbertFilter() const;
    
    /**
     * @brief Apply Hilbert transform directly to FFT data
     * @param fftData FFT data (modified in-place)
     */
    void applyToFFT(ComplexArray& fftData) const;
    
    /**
     * @brief Get block size
     */
    size_t getBlockSize() const { return blockSize_; }

private:
    size_t blockSize_;
    ComplexArray hilbertFilter_;
    
    void initializeFilter();
};

} // namespace rf
} // namespace vhsdecode
