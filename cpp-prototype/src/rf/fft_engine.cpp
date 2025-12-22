#define _USE_MATH_DEFINES
#define NOMINMAX
#include "vhsdecode/fft_engine.hpp"
#include <stdexcept>
#include <cmath>
#include <algorithm>

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#ifdef HAVE_CLFFT
#include "vhsdecode/clfft_engine.hpp"
#include "vhsdecode/opencl_context.hpp"
#endif

namespace vhsdecode {
namespace rf {

#ifdef HAVE_FFTW3
// FFTW3-based FFT implementation (production speed)
class FFTEngine::Impl {
public:
    size_t blockSize;
    bool useGPU;
    
    // FFTW3 plans and buffers
    fftwf_plan forwardPlan;
    fftwf_plan inversePlan;
    float* realBuffer;
    fftwf_complex* complexBuffer;

#ifdef HAVE_CLFFT
    // GPU path - use pointers to avoid initialization when GPU not requested
    std::unique_ptr<gpu::OpenCLContext> clContext;
    std::unique_ptr<gpu::CLFFTEngine> clfftEngine;
#endif
    
    Impl(size_t size, bool gpu) : blockSize(size), useGPU(gpu) {
        // Allocate aligned memory for FFTW3
        realBuffer = fftwf_alloc_real(blockSize);
        complexBuffer = fftwf_alloc_complex(blockSize / 2 + 1);
        
        if (!realBuffer || !complexBuffer) {
            throw std::runtime_error("Failed to allocate FFTW3 buffers");
        }
        
        // Create FFTW3 plans (FFTW_MEASURE for optimal performance)
        forwardPlan = fftwf_plan_dft_r2c_1d(
            static_cast<int>(blockSize),
            realBuffer,
            complexBuffer,
            FFTW_MEASURE
        );
        
        inversePlan = fftwf_plan_dft_c2r_1d(
            static_cast<int>(blockSize),
            complexBuffer,
            realBuffer,
            FFTW_MEASURE
        );
        
        if (!forwardPlan || !inversePlan) {
            fftwf_free(realBuffer);
            fftwf_free(complexBuffer);
            throw std::runtime_error("Failed to create FFTW3 plans");
        }

#ifdef HAVE_CLFFT
        if (useGPU) {
            // Initialize GPU FFT engine; fallback is handled by caller on exception
            clContext = std::make_unique<gpu::OpenCLContext>();
            clfftEngine = std::make_unique<gpu::CLFFTEngine>(*clContext, blockSize, true);
        }
#endif
    }
    
    ~Impl() {
        if (forwardPlan) fftwf_destroy_plan(forwardPlan);
        if (inversePlan) fftwf_destroy_plan(inversePlan);
        if (realBuffer) fftwf_free(realBuffer);
        if (complexBuffer) fftwf_free(complexBuffer);
    }
};

#else
// Prototype FFT implementation (slow, for testing without FFTW3)
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
#endif

FFTEngine::FFTEngine(size_t blockSize, bool useGPU)
    : impl_(std::make_unique<Impl>(blockSize, useGPU))
    , blockSize_(blockSize)
    , useGPU_(useGPU)
{
}

FFTEngine::~FFTEngine() = default;

FFTEngine::FFTEngine(FFTEngine&&) noexcept = default;
FFTEngine& FFTEngine::operator=(FFTEngine&&) noexcept = default;

#ifndef HAVE_FFTW3
// Simple Cooley-Tukey FFT implementation for prototype (only used without FFTW3)
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
#endif

ComplexArray FFTEngine::forwardFFT(const RealArray& input) {
    if (input.size() != blockSize_) {
        throw std::invalid_argument("Input size doesn't match block size");
    }
    
#if defined(HAVE_CLFFT) && defined(HAVE_FFTW3)
    // GPU path using clFFT when enabled
    if (useGPU_ && impl_->clfftEngine) {
        // Convert float input to double for clFFT API
        std::vector<double> inputD(input.begin(), input.end());
        auto gpuOut = impl_->clfftEngine->forwardFFT(inputD);
        ComplexArray result(gpuOut.size());
        for (size_t i = 0; i < gpuOut.size(); ++i) {
            result[i] = std::complex<float>(
                static_cast<float>(gpuOut[i].real()),
                static_cast<float>(gpuOut[i].imag())
            );
        }
        return result;
    }
#endif

#ifdef HAVE_FFTW3
    // FFTW3 path (fast)
    // Copy input to FFTW buffer
    std::copy(input.begin(), input.end(), impl_->realBuffer);
    
    // Execute FFT
    fftwf_execute(impl_->forwardPlan);
    
    // Convert FFTW complex to std::complex
    ComplexArray result(blockSize_/2 + 1);
    for (size_t i = 0; i <= blockSize_/2; ++i) {
        result[i] = std::complex<float>(
            impl_->complexBuffer[i][0],
            impl_->complexBuffer[i][1]
        );
    }
    
    return result;
#else
    // Prototype path (slow)
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
#endif
}

RealArray FFTEngine::inverseFFT(const ComplexArray& input) {
    size_t expectedSize = blockSize_/2 + 1;
    if (input.size() != expectedSize) {
        throw std::invalid_argument("Input size doesn't match expected FFT size");
    }
    
#if defined(HAVE_CLFFT) && defined(HAVE_FFTW3)
    if (useGPU_ && impl_->clfftEngine) {
        // Convert float complex to double complex for clFFT API
        std::vector<std::complex<double>> inputD(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            inputD[i] = std::complex<double>(input[i].real(), input[i].imag());
        }
        auto gpuOut = impl_->clfftEngine->inverseFFT(inputD);
        RealArray result(gpuOut.size());
        for (size_t i = 0; i < gpuOut.size(); ++i) {
            result[i] = static_cast<float>(gpuOut[i]);
        }
        return result;
    }
#endif

#ifdef HAVE_FFTW3
    // FFTW3 path (fast)
    // Copy input to FFTW buffer
    for (size_t i = 0; i <= blockSize_/2; ++i) {
        impl_->complexBuffer[i][0] = input[i].real();
        impl_->complexBuffer[i][1] = input[i].imag();
    }
    
    // Execute inverse FFT
    fftwf_execute(impl_->inversePlan);
    
    // Copy result and normalize
    RealArray result(blockSize_);
    float norm = 1.0f / blockSize_;
    for (size_t i = 0; i < blockSize_; ++i) {
        result[i] = impl_->realBuffer[i] * norm;
    }
    
    return result;
#else
    // Prototype path (slow)
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
#endif
}

void FFTEngine::applyFilter(ComplexArray& fftData, const ComplexArray& filter) {
    if (fftData.size() != filter.size()) {
        throw std::invalid_argument("FFT data and filter sizes must match");
    }

#if defined(HAVE_CLFFT) && defined(HAVE_FFTW3)
    // Currently apply filter on CPU even when GPU is enabled to keep behaviour consistent.
    // TODO: move to FilterKernel once integrated.
#endif
    for (size_t i = 0; i < fftData.size(); ++i) {
        fftData[i] *= filter[i];
    }
}

} // namespace rf
} // namespace vhsdecode

