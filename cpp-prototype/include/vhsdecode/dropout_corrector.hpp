#pragma once

#include <vector>
#include <cstddef>

namespace vhsdecode {
namespace video {

/**
 * @brief Detects and corrects RF signal dropouts
 * 
 * Uses envelope analysis to detect dropouts and linear interpolation
 * to correct them.
 */
class DropoutCorrector {
public:
    struct DropoutRegion {
        size_t start;  // Start sample index
        size_t end;    // End sample index
        
        size_t length() const { return end - start; }
    };

    struct CorrectionResult {
        std::vector<double> correctedSignal;
        std::vector<DropoutRegion> dropouts;
        size_t totalDropoutSamples;
    };

    /**
     * @brief Construct dropout corrector
     * @param threshold Envelope threshold for dropout detection (default: 0.3)
     */
    explicit DropoutCorrector(double threshold = 0.3);

    /**
     * @brief Detect and correct dropouts
     * @param signal Input signal to correct
     * @param envelope Envelope of the signal for dropout detection
     * @return Corrected signal and dropout statistics
     */
    CorrectionResult detectAndCorrect(
        const std::vector<double>& signal,
        const std::vector<double>& envelope
    );

    double getThreshold() const { return threshold_; }
    void setThreshold(double threshold) { threshold_ = threshold; }

private:
    double threshold_;

    std::vector<DropoutRegion> detectDropouts(const std::vector<double>& envelope);
    std::vector<double> correctDropouts(
        const std::vector<double>& signal,
        const std::vector<DropoutRegion>& dropouts
    );
};

} // namespace video
} // namespace vhsdecode
