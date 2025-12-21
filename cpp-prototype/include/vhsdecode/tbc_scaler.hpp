#ifndef VHSDECODE_TBC_SCALER_HPP
#define VHSDECODE_TBC_SCALER_HPP

#include "vhsdecode/types.hpp"
#include <vector>
#include <cstdint>

namespace vhsdecode {
namespace tbc {

/**
 * TBC Scaler - Time-Base Correction scaling
 * 
 * Resamples video lines from input sample rate to output sample rate
 * using bicubic interpolation (matching Python's scale() function).
 * 
 * For PAL:
 *   Input:  2560 samples/line at 40 MHz
 *   Output: 1135 samples/line at 4×fsc (17.734475 MHz)
 */
class TBCScaler {
public:
    struct Config {
        // Input parameters
        double inputSampleRateMHz = 40.0;
        double linePeriodUS = 64.0;  // PAL: 64µs, NTSC: 63.556µs
        
        // Output parameters  
        int outputLineLength = 1135;  // PAL: 1135, NTSC: 910
        double outputSampleRateMHz = 17.734475;  // 4 × fsc_pal
        
        // Derived
        int inputLineLength() const {
            return static_cast<int>(linePeriodUS * inputSampleRateMHz);
        }
    };
    
    explicit TBCScaler(const Config& config);
    ~TBCScaler();
    
    // Non-copyable
    TBCScaler(const TBCScaler&) = delete;
    TBCScaler& operator=(const TBCScaler&) = delete;
    
    // Movable
    TBCScaler(TBCScaler&&) noexcept;
    TBCScaler& operator=(TBCScaler&&) noexcept;
    
    /**
     * Scale a single line using bicubic interpolation
     * @param input Input line data (at input sample rate)
     * @param begin Start position (fractional samples)
     * @param end End position (fractional samples)
     * @return Scaled line at output sample rate
     */
    RealArray scaleLine(const RealArray& input, double begin, double end);
    
    /**
     * Scale a single line from fixed positions
     * @param input Input line data  
     * @param lineStart Start sample index
     * @param lineLength Length of input line in samples
     * @return Scaled line at output sample rate
     */
    RealArray scaleLine(const RealArray& input, size_t lineStart, size_t lineLength);
    
    /**
     * Scale an entire field
     * @param input Full field data at input sample rate
     * @param lineStarts Start positions for each line
     * @param numLines Number of lines to extract
     * @return Scaled field data (numLines × outputLineLength)
     */
    std::vector<RealArray> scaleField(
        const RealArray& input,
        const std::vector<size_t>& lineStarts,
        size_t numLines);
    
    /**
     * Get configuration
     */
    const Config& getConfig() const { return config_; }
    
private:
    Config config_;
    
    /**
     * Bicubic interpolation helper
     * Originally from https://www.paulinternet.nl/?page=bicubic
     */
    static float bicubicInterpolate(float p0, float p1, float p2, float p3, float x);
};

} // namespace tbc
} // namespace vhsdecode

#endif // VHSDECODE_TBC_SCALER_HPP
