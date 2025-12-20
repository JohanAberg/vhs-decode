#pragma once

#include "vhsdecode/types.hpp"
#include <vector>
#include <complex>
#include <memory>

namespace vhsdecode {

// Forward declarations
namespace demod { class FMDemodulator; }
namespace video {
    class ChromaSeparator;
    class DropoutCorrector;
    class TBCCorrector;
}

namespace video {

/**
 * @brief Orchestrates complete video processing pipeline
 * 
 * Coordinates FM demodulation, dropout correction, chroma separation,
 * and time-base correction.
 */
class VideoProcessor {
public:
    struct ProcessingResult {
        std::vector<double> video;
        std::vector<double> chroma;
        std::vector<std::vector<double>> fields;
        size_t dropoutCount;
        size_t totalLines;
        double processingTimeMs;
    };

    struct Config {
        TVSystem system;
        double sampleRate;
        double dropoutThreshold;
    };

    /**
     * @brief Construct video processor
     * @param config Processing configuration
     */
    explicit VideoProcessor(const Config& config);

    /**
     * @brief Destructor
     */
    ~VideoProcessor();

    /**
     * @brief Process analytic signal through complete video pipeline
     * @param analyticSignal Complex analytic signal from RF processor
     * @param envelope Envelope for dropout detection
     * @return Processing result with video, chroma, and fields
     */
    ProcessingResult processAnalyticSignal(
        const std::vector<std::complex<double>>& analyticSignal,
        const std::vector<double>& envelope
    );

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace video
} // namespace vhsdecode
