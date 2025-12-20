#pragma once

#include <vector>
#include <memory>

namespace vhsdecode {

// Forward declarations
class FilterBank;
class FFTWEngine;

namespace video {

/**
 * @brief Separates video (luminance) and chroma (chrominance) components
 * 
 * Uses frequency-domain filtering to separate Y and C components from
 * demodulated FM signal. For VHS, this implements color-under separation.
 */
class ChromaSeparator {
public:
    struct SeparationResult {
        std::vector<double> video;   // Luminance (Y) component
        std::vector<double> chroma;  // Chrominance (C) component
    };

    /**
     * @brief Construct chroma separator
     * @param filterBank Reference to filter bank with video_lpf and chroma_bp filters
     * @param fftEngine Reference to FFT engine for filtering
     */
    ChromaSeparator(FilterBank& filterBank, FFTWEngine& fftEngine);
    
    ~ChromaSeparator();

    /**
     * @brief Separate video and chroma components
     * @param demodulated Demodulated FM signal
     * @return Separated video and chroma components
     */
    SeparationResult separate(const std::vector<double>& demodulated);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace video
} // namespace vhsdecode
