#define _USE_MATH_DEFINES
#include "vhsdecode/rf_processor.hpp"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace vhsdecode {
namespace rf {

RFProcessor::RFProcessor(const Config& config)
    : config_(config)
{
    // Validate configuration
    if (config_.blockSize == 0 || (config_.blockSize & (config_.blockSize - 1)) != 0) {
        throw std::invalid_argument("Block size must be a power of 2");
    }
    
    if (config_.rfBandpassHighMHz > config_.sampleRateMHz / 2.0) {
        throw std::invalid_argument("Bandpass high frequency exceeds Nyquist frequency");
    }
    
    // Initialize components
    fftEngine_ = std::make_unique<FFTEngine>(config_.blockSize, config_.useGPU);
    filterBank_ = std::make_unique<FilterBank>(config_.sampleRateMHz, config_.blockSize);
    
    // Create RF bandpass filter matching Python VHS PAL configuration:
    // Python uses separate HPF and LPF with high orders instead of a single BPF
    // - video_hpf_extra = 1.32 MHz, order 12
    // - video_lpf_extra = 5.91 MHz, order 20
    filterBank_->createButterworthLPF("rf_lpf", 5.91, 20);  // 5.91 MHz LPF order 20
    filterBank_->createButterworthHPF("rf_hpf", 1.32, 12);  // 1.32 MHz HPF order 12
    
    // Create combined bandpass filter by multiplying HPF * LPF
    filterBank_->createCombinedFilter("rf_bandpass", "rf_hpf", "rf_lpf");

    // Fuse Hilbert transform into bandpass filter
    // Get the bandpass filter
    auto bandpass = filterBank_->getFilter("rf_bandpass");
    
    // Apply Hilbert transform (multiply by 2 for positive frequencies)
    // Note: Hilbert filter is [1, 2, 2, ..., 2, 1] (DC, pos, ..., pos, Nyquist)
    for (size_t i = 0; i < bandpass.size(); ++i) {
        if (i > 0 && i < bandpass.size() - 1) {
            bandpass[i] *= 2.0f;
        }
    }
    
    // Save as "rf_analytic"
    filterBank_->setFilter("rf_analytic", bandpass);
}

RFProcessor::~RFProcessor() = default;

RFProcessor::RFProcessor(RFProcessor&&) noexcept = default;
RFProcessor& RFProcessor::operator=(RFProcessor&&) noexcept = default;

void RFProcessor::setBandpassFrequencies(double lowMHz, double highMHz) {
    config_.rfBandpassLowMHz = lowMHz;
    config_.rfBandpassHighMHz = highMHz;
    
    // Recreate filters
    filterBank_->createButterworthLPF("rf_lpf", highMHz, 20);
    filterBank_->createButterworthHPF("rf_hpf", lowMHz, 12);
    filterBank_->createCombinedFilter("rf_bandpass", "rf_hpf", "rf_lpf");

    // Fuse Hilbert transform
    auto bandpass = filterBank_->getFilter("rf_bandpass");
    for (size_t i = 0; i < bandpass.size(); ++i) {
        if (i > 0 && i < bandpass.size() - 1) {
            bandpass[i] *= 2.0f;
        }
    }
    filterBank_->setFilter("rf_analytic", bandpass);
}

RealArray RFProcessor::uint8ToFloat(const std::vector<uint8_t>& data) {
    RealArray result(data.size());
    
    // Convert uint8 to float, normalize to [-1, 1]
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = (static_cast<float>(data[i]) - 128.0f) / 128.0f;
    }
    
    return result;
}

RFProcessor::Result RFProcessor::processBlock(const RealArray& rfData) {
    if (rfData.size() != config_.blockSize) {
        throw std::invalid_argument("RF data size doesn't match block size");
    }
    
    // Debug: print input data (only for first few calls)
    static int debugCount = 0;
    if (debugCount < 2) {
        std::cout << "\n=== RF Input Debug (call #" << debugCount << ") ===\n";
        std::cout << "Input first 10: [";
        for (size_t i = 0; i < 10 && i < rfData.size(); ++i) {
            std::cout << std::fixed << std::setprecision(4) << rfData[i];
            if (i < 9) std::cout << ", ";
        }
        std::cout << "]\n";
        debugCount++;
    }
    
    Result result;
    
    // Step 1: Forward FFT (real to complex)
    auto fftData = fftEngine_->forwardFFT(rfData);
    
    // Step 2: Apply fused Analytic Filter (Bandpass + Hilbert)
    const auto& analyticFilter = filterBank_->getFilter("rf_analytic");
    fftEngine_->applyFilter(fftData, analyticFilter);
    
    // Step 3: Inverse FFT (complex to complex)
    // We need to construct full spectrum for C2C IFFT
    // Since we have analytic signal (negative freqs = 0), we just pad with zeros.
    size_t N = config_.blockSize;
    ComplexArray fullSpectrum(N);
    
    // Copy positive frequencies
    for (size_t i = 0; i <= N/2; ++i) {
        fullSpectrum[i] = fftData[i];
    }
    // Negative frequencies are already 0 (default constructor)

    // Perform C2C IFFT
    result.analyticSignal = fftEngine_->complexInverseFFT(fullSpectrum);
    
    // Debug: print analytic signal
    static int debugAnalyticCount = 0;
    if (debugAnalyticCount < 2) {
        std::cout << "=== RF Processor Debug (call #" << debugAnalyticCount << ") ===\n";
        std::cout << "Analytic signal first 10: [";
        for (size_t i = 0; i < 10 && i < N; ++i) {
            std::cout << "(" << std::fixed << std::setprecision(4) << result.analyticSignal[i].real() << "+" 
                      << result.analyticSignal[i].imag() << "j)";
            if (i < 9) std::cout << ", ";
        }
        std::cout << "]\n";
        
        // Compute and print phase angles
        std::cout << "Phase angles first 10: [";
        for (size_t i = 0; i < 10 && i < N; ++i) {
            std::cout << std::fixed << std::setprecision(4) << std::arg(result.analyticSignal[i]);
            if (i < 9) std::cout << ", ";
        }
        std::cout << "]\n";
        debugAnalyticCount++;
    }
    
    // Step 5: Compute envelope
    result.envelope.resize(N);
    for (size_t i = 0; i < N; ++i) {
        result.envelope[i] = std::abs(result.analyticSignal[i]);
    }
    
    // Step 6: Extract filtered real signal
    result.filtered.resize(N);
    for (size_t i = 0; i < N; ++i) {
        result.filtered[i] = result.analyticSignal[i].real();
    }
    
    return result;
}

RFProcessor::Result RFProcessor::processBlock(const std::vector<uint8_t>& rfSamples) {
    auto rfData = uint8ToFloat(rfSamples);
    return processBlock(rfData);
}

} // namespace rf
} // namespace vhsdecode
