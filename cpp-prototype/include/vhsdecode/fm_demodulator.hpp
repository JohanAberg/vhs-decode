#pragma once

#include "vhsdecode/types.hpp"
#include <memory>

namespace vhsdecode {
namespace demod {

/**
 * @brief FM demodulator for RF video signals
 * 
 * Performs FM demodulation including phase unwrapping, envelope detection,
 * and spike replacement.
 */
class FMDemodulator {
public:
    /**
     * @brief Demodulation result
     */
    struct Result {
        RealArray video;       // Main video signal
        RealArray video05;     // 0.5 MHz filtered video  
        RealArray chroma;      // Chroma carrier
        RealArray envelope;    // Envelope for dropout detection
    };
    
    /**
     * @brief Create FM demodulator
     * @param config Processing configuration
     */
    explicit FMDemodulator(const ProcessingConfig& config);
    
    /**
     * @brief Destructor
     */
    ~FMDemodulator();
    
    // Disable copy, allow move
    FMDemodulator(const FMDemodulator&) = delete;
    FMDemodulator& operator=(const FMDemodulator&) = delete;
    FMDemodulator(FMDemodulator&&) noexcept;
    FMDemodulator& operator=(FMDemodulator&&) noexcept;
    
    /**
     * @brief Demodulate RF signal from analytic signal
     * @param analyticSignal Complex analytic signal (from Hilbert transform)
     * @return Demodulation result with video, chroma, envelope
     */
    Result demodulate(const ComplexArray& analyticSignal);
    
    /**
     * @brief Get configuration
     */
    const ProcessingConfig& getConfig() const { return config_; }

private:
    class Impl;  // Forward declaration for pimpl
    std::unique_ptr<Impl> impl_;
    
    ProcessingConfig config_;
};

} // namespace demod
} // namespace vhsdecode
