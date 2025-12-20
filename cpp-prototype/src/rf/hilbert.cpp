#include "vhsdecode/hilbert.hpp"
#include <cmath>

namespace vhsdecode {
namespace rf {

HilbertTransform::HilbertTransform(size_t blockSize)
    : blockSize_(blockSize)
{
    initializeFilter();
}

HilbertTransform::~HilbertTransform() = default;

HilbertTransform::HilbertTransform(HilbertTransform&&) noexcept = default;
HilbertTransform& HilbertTransform::operator=(HilbertTransform&&) noexcept = default;

void HilbertTransform::initializeFilter() {
    size_t numBins = blockSize_ / 2 + 1;
    hilbertFilter_.resize(numBins);
    
    // Hilbert transform in frequency domain:
    // - Multiplies positive frequencies by 2 (emphasizes them)
    // - Keeps DC and Nyquist unchanged
    // - Effectively creates analytic signal (no negative frequencies)
    
    for (size_t i = 0; i < numBins; ++i) {
        if (i == 0 || i == blockSize_ / 2) {
            // DC and Nyquist: multiply by 1
            hilbertFilter_[i] = std::complex<float>(1.0f, 0.0f);
        } else {
            // Positive frequencies: multiply by 2
            hilbertFilter_[i] = std::complex<float>(2.0f, 0.0f);
        }
    }
}

ComplexArray HilbertTransform::getHilbertFilter() const {
    return hilbertFilter_;
}

void HilbertTransform::applyToFFT(ComplexArray& fftData) const {
    if (fftData.size() != hilbertFilter_.size()) {
        throw std::invalid_argument("FFT data size doesn't match Hilbert filter size");
    }
    
    // Element-wise multiplication
    for (size_t i = 0; i < fftData.size(); ++i) {
        fftData[i] *= hilbertFilter_[i];
    }
}

} // namespace rf
} // namespace vhsdecode
