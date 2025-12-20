#include "vhsdecode/rf_processor.hpp"
#include <stdexcept>
#include <cmath>

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
    hilbert_ = std::make_unique<HilbertTransform>(config_.blockSize);
    
    // Create default RF bandpass filter
    filterBank_->createBandpassFilter("rf_bandpass", 
                                     config_.rfBandpassLowMHz,
                                     config_.rfBandpassHighMHz);
}

RFProcessor::~RFProcessor() = default;

RFProcessor::RFProcessor(RFProcessor&&) noexcept = default;
RFProcessor& RFProcessor::operator=(RFProcessor&&) noexcept = default;

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
    
    Result result;
    
    // Step 1: Forward FFT (real to complex)
    auto fftData = fftEngine_->forwardFFT(rfData);
    
    // Step 2: Apply RF bandpass filter
    const auto& rfFilter = filterBank_->getFilter("rf_bandpass");
    fftEngine_->applyFilter(fftData, rfFilter);
    
    // Step 3: Apply Hilbert transform (creates analytic signal)
    hilbert_->applyToFFT(fftData);
    
    // Step 4: Inverse FFT (complex to real for filtered signal)
    // But we want the complex analytic signal, so we need to handle this differently
    // For now, store the filtered FFT data
    
    // Convert FFT to complex time-domain (analytic signal)
    // This requires a complex iFFT, which our simple implementation doesn't have yet
    // For the prototype, we'll compute it manually
    
    size_t N = config_.blockSize;
    result.analyticSignal.resize(N);
    
    // Reconstruct full complex spectrum for inverse FFT
    ComplexArray fullSpectrum(N);
    for (size_t i = 0; i <= N/2; ++i) {
        fullSpectrum[i] = fftData[i];
    }
    // Mirror for negative frequencies (conjugate symmetry for real signals)
    // But for Hilbert transform, we zero the negative frequencies
    for (size_t i = N/2 + 1; i < N; ++i) {
        fullSpectrum[i] = std::complex<float>(0.0f, 0.0f);
    }
    
    // Simple inverse DFT (TODO: use proper complex iFFT)
    for (size_t n = 0; n < N; ++n) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t k = 0; k < N; ++k) {
            float angle = 2.0f * M_PI * k * n / N;
            std::complex<float> twiddle(std::cos(angle), std::sin(angle));
            sum += fullSpectrum[k] * twiddle;
        }
        result.analyticSignal[n] = sum / static_cast<float>(N);
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
