#pragma once

#include "vhsdecode/types.hpp"
#include "vhsdecode/rf_processor.hpp"
#include "vhsdecode/fm_demodulator.hpp"
#include <memory>
#include <vector>

namespace vhsdecode {

/**
 * @brief Main processing pipeline
 * 
 * Orchestrates the flow of data through the decoder stages:
 * RF Processing -> FM Demodulation -> Video Filtering -> TBC
 */
class Pipeline {
public:
    struct Config {
        double inputFreqMHz;
        size_t blockSize;
        bool useGPU;
        TVSystem system;
        TapeFormat format = TapeFormat::VHS;
    };

    explicit Pipeline(const Config& config);
    ~Pipeline();

    // Disable copy
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // Configuration
    void setBandpassFrequencies(double lowMHz, double highMHz);
    void setSpikeThreshold(float mhz);
    void setSpikeReplacement(bool enable);
    
    /**
     * @brief Configure video low-pass filter
     * @param cutoffMHz Cutoff frequency in MHz
     * @param order Filter order (e.g. 6)
     */
    void setVideoLPF(double cutoffMHz, int order);

    /**
     * @brief Configure video de-emphasis filter
     * @param gain Gain in dB
     * @param midFreqHz Center frequency in Hz
     * @param q Q factor
     */
    void setDeemphasis(double gain, double midFreqHz, double q);
    
    // Processing
    // Returns true if successful
    // videoOutput and chromaOutput are appended to
    bool processBlock(const std::vector<uint8_t>& inputBlock, 
                      std::vector<uint16_t>& videoOutput,
                      std::vector<uint16_t>& chromaOutput);

    // Accessors
    rf::RFProcessor& getRFProcessor();
    demod::FMDemodulator& getFMDemodulator();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vhsdecode
