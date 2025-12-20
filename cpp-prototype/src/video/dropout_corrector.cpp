#include "vhsdecode/dropout_corrector.hpp"
#include <iostream>
#include <algorithm>

namespace vhsdecode {
namespace video {

DropoutCorrector::DropoutCorrector(double threshold)
    : threshold_(threshold) {
    std::cout << "✓ Dropout corrector initialized" << std::endl;
    std::cout << "  Threshold: " << threshold_ << std::endl;
}

std::vector<DropoutCorrector::DropoutRegion>
DropoutCorrector::detectDropouts(const std::vector<double>& envelope) {
    std::vector<DropoutRegion> dropouts;
    
    bool inDropout = false;
    size_t dropoutStart = 0;
    
    for (size_t i = 0; i < envelope.size(); ++i) {
        if (envelope[i] < threshold_) {
            if (!inDropout) {
                // Start of dropout region
                dropoutStart = i;
                inDropout = true;
            }
        } else {
            if (inDropout) {
                // End of dropout region
                dropouts.push_back({dropoutStart, i});
                inDropout = false;
            }
        }
    }
    
    // Handle dropout at end of signal
    if (inDropout) {
        dropouts.push_back({dropoutStart, envelope.size()});
    }
    
    return dropouts;
}

std::vector<double>
DropoutCorrector::correctDropouts(
    const std::vector<double>& signal,
    const std::vector<DropoutRegion>& dropouts
) {
    std::vector<double> corrected = signal;
    
    for (const auto& dropout : dropouts) {
        if (dropout.start == 0 || dropout.end >= signal.size()) {
            // Can't interpolate at boundaries - just zero it
            std::fill(corrected.begin() + dropout.start,
                     corrected.begin() + dropout.end,
                     0.0);
            continue;
        }
        
        // Linear interpolation from last good sample to next good sample
        double startValue = signal[dropout.start - 1];
        double endValue = signal[dropout.end];
        size_t length = dropout.length();
        
        for (size_t i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / static_cast<double>(length);
            corrected[dropout.start + i] = startValue + t * (endValue - startValue);
        }
    }
    
    return corrected;
}

DropoutCorrector::CorrectionResult
DropoutCorrector::detectAndCorrect(
    const std::vector<double>& signal,
    const std::vector<double>& envelope
) {
    CorrectionResult result;
    
    // Detect dropouts
    result.dropouts = detectDropouts(envelope);
    
    // Calculate total dropout samples
    result.totalDropoutSamples = 0;
    for (const auto& dropout : result.dropouts) {
        result.totalDropoutSamples += dropout.length();
    }
    
    // Correct dropouts
    result.correctedSignal = correctDropouts(signal, result.dropouts);
    
    // Print statistics
    std::cout << "✓ Dropout detection & correction complete" << std::endl;
    std::cout << "  Dropouts found: " << result.dropouts.size() << " regions" << std::endl;
    std::cout << "  Total samples affected: " << result.totalDropoutSamples 
              << " (" << (100.0 * result.totalDropoutSamples / signal.size()) << "%)" << std::endl;
    
    return result;
}

} // namespace video
} // namespace vhsdecode
