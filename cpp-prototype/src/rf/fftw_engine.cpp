#include "vhsdecode/fftw_engine.hpp"
#include <fftw3.h>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <iostream>

namespace vhsdecode {
namespace rf {

namespace {
    // Global wisdom management
    std::mutex wisdomMutex;
    bool wisdomLoaded = false;
    
    void loadWisdom() {
        std::lock_guard<std::mutex> lock(wisdomMutex);
        if (!wisdomLoaded) {
            // Try to load wisdom from file for faster planning
            const char* wisdomFile = "vhs-decode-fftw-wisdom.dat";
            if (fftw_import_wisdom_from_filename(wisdomFile) != 0) {
                std::cout << "[FFTW] Loaded wisdom from " << wisdomFile << std::endl;
            }
            wisdomLoaded = true;
        }
    }
    
    void saveWisdom() {
        std::lock_guard<std::mutex> lock(wisdomMutex);
        const char* wisdomFile = "vhs-decode-fftw-wisdom.dat";
        if (fftw_export_wisdom_to_filename(wisdomFile) != 0) {
            std::cout << "[FFTW] Saved wisdom to " << wisdomFile << std::endl;
        }
    }
}

// Pimpl implementation holding FFTW3 internals
struct FFTWEngine::Impl {
    fftw_plan forwardPlan;
    fftw_plan inversePlan;
    double* realInput;
    fftw_complex* complexOutput;
    fftw_complex* complexInput;
    double* realOutput;
    size_t blockSize;
    
    Impl(size_t size) : blockSize(size) {
        // Load FFTW wisdom if not already loaded
        loadWisdom();
        
        // Allocate aligned memory for SIMD optimization
        realInput = fftw_alloc_real(blockSize);
        complexOutput = fftw_alloc_complex(blockSize / 2 + 1);
        complexInput = fftw_alloc_complex(blockSize / 2 + 1);
        realOutput = fftw_alloc_real(blockSize);
        
        if (!realInput || !complexOutput || !complexInput || !realOutput) {
            cleanup();
            throw std::runtime_error("Failed to allocate FFTW memory");
        }
        
        // Create FFTW plans using PATIENT mode for first-time optimization
        // If wisdom exists, this will be fast. Otherwise it will measure and save.
        unsigned flags = FFTW_PATIENT | FFTW_DESTROY_INPUT;
        
        forwardPlan = fftw_plan_dft_r2c_1d(
            static_cast<int>(blockSize),
            realInput,
            complexOutput,
            flags
        );
        
        inversePlan = fftw_plan_dft_c2r_1d(
            static_cast<int>(blockSize),
            complexInput,
            realOutput,
            flags
        );
        
        if (!forwardPlan || !inversePlan) {
            cleanup();
            throw std::runtime_error("Failed to create FFTW plans");
        }
        
        // Save wisdom after creating plans (only if new plans were measured)
        saveWisdom();
    }
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        if (forwardPlan) fftw_destroy_plan(forwardPlan);
        if (inversePlan) fftw_destroy_plan(inversePlan);
        if (realInput) fftw_free(realInput);
        if (complexOutput) fftw_free(complexOutput);
        if (complexInput) fftw_free(complexInput);
        if (realOutput) fftw_free(realOutput);
        
        forwardPlan = nullptr;
        inversePlan = nullptr;
        realInput = nullptr;
        complexOutput = nullptr;
        complexInput = nullptr;
        realOutput = nullptr;
    }
};

FFTWEngine::FFTWEngine(size_t blockSize, bool useGPU)
    : blockSize_(blockSize), useGPU_(useGPU) {
    
    // Validate block size is power of 2
    if (blockSize == 0 || (blockSize & (blockSize - 1)) != 0) {
        throw std::invalid_argument("Block size must be a power of 2");
    }
    
    if (useGPU) {
        // GPU support not yet implemented - fall back to CPU
        useGPU_ = false;
    }
    
    // Create implementation
    impl_ = std::make_unique<Impl>(blockSize);
}

FFTWEngine::~FFTWEngine() = default;

FFTWEngine::FFTWEngine(FFTWEngine&&) noexcept = default;
FFTWEngine& FFTWEngine::operator=(FFTWEngine&&) noexcept = default;

std::vector<std::complex<double>> FFTWEngine::forwardFFT(const std::vector<double>& input) {
    if (input.size() != blockSize_) {
        throw std::invalid_argument("Input size does not match block size");
    }
    
    // Copy input to FFTW buffer
    std::copy(input.begin(), input.end(), impl_->realInput);
    
    // Execute forward FFT
    fftw_execute(impl_->forwardPlan);
    
    // Convert FFTW complex to std::complex
    size_t binCount = blockSize_ / 2 + 1;
    std::vector<std::complex<double>> result(binCount);
    
    for (size_t i = 0; i < binCount; ++i) {
        result[i] = std::complex<double>(
            impl_->complexOutput[i][0],
            impl_->complexOutput[i][1]
        );
    }
    
    return result;
}

std::vector<double> FFTWEngine::inverseFFT(const std::vector<std::complex<double>>& input) {
    size_t expectedBins = blockSize_ / 2 + 1;
    if (input.size() != expectedBins) {
        throw std::invalid_argument("Input size does not match expected bin count");
    }
    
    // Copy input to FFTW buffer
    for (size_t i = 0; i < input.size(); ++i) {
        impl_->complexInput[i][0] = input[i].real();
        impl_->complexInput[i][1] = input[i].imag();
    }
    
    // Execute inverse FFT
    fftw_execute(impl_->inversePlan);
    
    // Copy result and normalize (FFTW doesn't normalize inverse transform)
    std::vector<double> result(blockSize_);
    double normalization = 1.0 / static_cast<double>(blockSize_);
    
    for (size_t i = 0; i < blockSize_; ++i) {
        result[i] = impl_->realOutput[i] * normalization;
    }
    
    return result;
}

void FFTWEngine::applyFilter(std::vector<std::complex<double>>& fftData,
                             const std::vector<double>& filter) {
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
