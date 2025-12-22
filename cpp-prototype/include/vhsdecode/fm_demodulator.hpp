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
     * @brief Reset internal state (e.g. for seeking or restarting)
     */
    void reset();

    /**
     * @brief Set spike detection threshold
     * @param thresholdMHz Threshold in MHz (e.g. 9.6 for 2x white level)
     */
    void setSpikeThreshold(float thresholdMHz);

    /**
     * @brief Get current spike detection threshold
     * @return Threshold in MHz
     */
    float getSpikeThreshold() const;

    /**
     * @brief Enable or disable spike replacement
     * @param enable True to enable
     */
    void setSpikeReplacement(bool enable);

    /**
     * @brief Check if spike replacement is enabled
     * @return True if enabled
     */
    bool isSpikeReplacementEnabled() const;
    
    /**
     * @brief Demodulate RF signal from analytic signal
     * @param analyticSignal Complex analytic signal (from Hilbert transform)
     * @return Demodulation result with video, chroma, envelope
     */
    Result demodulate(const ComplexArray& analyticSignal);

    /**
     * @brief Replace spikes in demodulated signal using differential demodulation
     * Uses configured threshold.
     * @param video Demodulated video signal (modified in-place)
     * @param analyticSignal Original analytic signal
     * @return Number of samples replaced
     */
    size_t replaceSpikes(RealArray& video, const ComplexArray& analyticSignal);

    /**
     * @brief Replace spikes in demodulated signal using differential demodulation
     * @param video Demodulated video signal (modified in-place)
     * @param analyticSignal Original analytic signal
     * @param threshold Frequency threshold for spike detection (Hz)
     * @return Number of samples replaced
     */
    size_t replaceSpikes(RealArray& video, const ComplexArray& analyticSignal, float threshold);
    
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
