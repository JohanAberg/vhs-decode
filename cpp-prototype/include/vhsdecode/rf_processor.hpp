#pragma once

#include "vhsdecode/types.hpp"
#include "vhsdecode/fft_engine.hpp"
#include "vhsdecode/filter_bank.hpp"
#include "vhsdecode/hilbert.hpp"
#include <memory>

namespace vhsdecode {
namespace rf {

/**
 * @brief RF Processor - orchestrates the RF signal processing pipeline
 * 
 * Pipeline: Raw RF → FFT → Bandpass Filter → Hilbert → iFFT → Analytic Signal
 */
class RFProcessor {
public:
    /**
     * @brief RF processing configuration
     */
    struct Config {
        double sampleRateMHz;
        size_t blockSize;
        double rfBandpassLowMHz;
        double rfBandpassHighMHz;
        bool useGPU;
        
        Config() 
            : sampleRateMHz(40.0)
            , blockSize(32768)
            , rfBandpassLowMHz(0.1)
            , rfBandpassHighMHz(18.0)
            , useGPU(false)
        {}
    };
    
    /**
     * @brief Processing result
     */
    struct Result {
        ComplexArray analyticSignal;  // Complex analytic signal
        RealArray envelope;            // Signal envelope
        RealArray filtered;            // Filtered real signal
        RealArray chroma;              // Extracted chroma signal (color-under)
    };
    
    /**
     * @brief Create RF processor
     * @param config Processing configuration
     */
    explicit RFProcessor(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~RFProcessor();
    
    // Disable copy, allow move
    RFProcessor(const RFProcessor&) = delete;
    RFProcessor& operator=(const RFProcessor&) = delete;
    RFProcessor(RFProcessor&&) noexcept;
    RFProcessor& operator=(RFProcessor&&) noexcept;
    
    /**
     * @brief Process RF block
     * @param rfData Raw RF samples (uint8 or float)
     * @return Processing result with analytic signal
     */
    Result processBlock(const RealArray& rfData);
    
    /**
     * @brief Process RF block from uint8 samples
     * @param rfSamples Raw RF samples (uint8)
     * @return Processing result
     */
    Result processBlock(const std::vector<uint8_t>& rfSamples);
    
    /**
     * @brief Get configuration
     */
    const Config& getConfig() const { return config_; }
    
    /**
     * @brief Get filter bank (for adding custom filters)
     */
    FilterBank& getFilterBank() { return *filterBank_; }
    
    /**
     * @brief Get FFT engine
     */
    FFTEngine& getFFTEngine() { return *fftEngine_; }

private:
    Config config_;
    
    std::unique_ptr<FFTEngine> fftEngine_;
    std::unique_ptr<FilterBank> filterBank_;
    std::unique_ptr<HilbertTransform> hilbert_;
    
    // Convert uint8 to float
    RealArray uint8ToFloat(const std::vector<uint8_t>& data);
};

} // namespace rf
} // namespace vhsdecode
