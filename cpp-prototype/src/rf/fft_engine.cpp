#include "vhsdecode/fft_engine.hpp"
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace vhsdecode {
namespace rf {

// Simple CPU-only FFT implementation for prototype
// TODO: Replace with FFTW3 for production, add clFFT for GPU
class FFTEngine::Impl {
public:
    size_t blockSize;
    bool useGPU;
    
    Impl(size_t size, bool gpu) : blockSize(size), useGPU(gpu) {
        // Check if block size is power of 2
        if ((size & (size - 1)) != 0) {
            throw std::invalid_argument("Block size must be power of 2");
        }
        
        if (useGPU) {
            // GPU not yet implemented
            throw std::runtime_error("GPU FFT not yet implemented");
        }
    }
};

FFTEngine::FFTEngine(size_t blockSize, bool useGPU)
    : impl_(std::make_unique<Impl>(blockSize, useGPU))
    , blockSize_(blockSize)
    , useGPU_(useGPU)
{
}

FFTEngine::~FFTEngine() = default;

FFTEngine::FFTEngine(FFTEngine&&) noexcept = default;
FFTEngine& FFTEngine::operator=(FFTEngine&&) noexcept = default;

// Simple Cooley-Tukey FFT implementation for prototype
static void fft_recursive(std::complex<float>* data, size_t n, bool inverse) {
    if (n <= 1) return;
    
    // Divide
    std::vector<std::complex<float>> even(n/2);
    std::vector<std::complex<float>> odd(n/2);
    
    for (size_t i = 0; i < n/2; ++i) {
        even[i] = data[i * 2];
        odd[i] = data[i * 2 + 1];
    }
    
    // Conquer
    fft_recursive(even.data(), n/2, inverse);
    fft_recursive(odd.data(), n/2, inverse);
    
    // Combine
    float sign = inverse ? 1.0f : -1.0f;
    for (size_t k = 0; k < n/2; ++k) {
        float angle = sign * 2.0f * M_PI * k / n;
        std::complex<float> t = std::polar(1.0f, angle) * odd[k];
        data[k] = even[k] + t;
        data[k + n/2] = even[k] - t;
    }
}

ComplexArray FFTEngine::forwardFFT(const RealArray& input) {
    if (input.size() != blockSize_) {
        throw std::invalid_argument("Input size doesn't match block size");
    }
    
    // Convert real to complex
    ComplexArray data(blockSize_);
    for (size_t i = 0; i < blockSize_; ++i) {
        data[i] = std::complex<float>(input[i], 0.0f);
    }
    
    // Perform FFT
    fft_recursive(data.data(), blockSize_, false);
    
    // Return only positive frequencies (N/2+1 bins)
    ComplexArray result(blockSize_/2 + 1);
    for (size_t i = 0; i <= blockSize_/2; ++i) {
        result[i] = data[i];
    }
    
    return result;
}

RealArray FFTEngine::inverseFFT(const ComplexArray& input) {
    size_t expectedSize = blockSize_/2 + 1;
    if (input.size() != expectedSize) {
        throw std::invalid_argument("Input size doesn't match expected FFT size");
    }
    
    // Reconstruct full complex spectrum (symmetric for real signals)
    ComplexArray data(blockSize_);
    for (size_t i = 0; i <= blockSize_/2; ++i) {
        data[i] = input[i];
    }
    // Mirror for negative frequencies
    for (size_t i = blockSize_/2 + 1; i < blockSize_; ++i) {
        data[i] = std::conj(data[blockSize_ - i]);
    }
    
    // Perform inverse FFT
    fft_recursive(data.data(), blockSize_, true);
    
    // Normalize and extract real part
    RealArray result(blockSize_);
    for (size_t i = 0; i < blockSize_; ++i) {
        result[i] = data[i].real() / blockSize_;
    }
    
    return result;
}

void FFTEngine::applyFilter(ComplexArray& fftData, const ComplexArray& filter) {
    if (fftData.size() != filter.size()) {
        throw std::invalid_argument("FFT data and filter sizes must match");
    }
    
    // Element-wise multiplication in frequency domain
    for (size_t i = 0; i < fftData.size(); ++i) {
        fftData[i] *= filter[i];
    }
}

} // namespace rf
} // namespace vhsdecode
