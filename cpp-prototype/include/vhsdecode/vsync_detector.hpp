#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <sys/types.h>  // For ssize_t on POSIX

namespace vhsdecode {
namespace sync {

/**
 * @brief Pulse types in the vertical blanking interval
 */
enum class VPulseType {
    UNKNOWN,
    HSYNC,      // Normal horizontal sync (~4.7µs)
    EQUALIZING, // Equalizing pulses (~2.35µs)
    VSYNC       // Vertical sync (broad, ~27µs)
};

/**
 * @brief Information about a detected pulse (vertical sync context)
 */
struct VPulse {
    size_t start;       // Start position in samples
    size_t end;         // End position in samples
    size_t length;      // Pulse length in samples
    VPulseType type;    // Classified pulse type
    float amplitude;    // Pulse depth (for sync quality)
};

/**
 * @brief Information about a detected field boundary
 */
struct FieldBoundary {
    size_t position;        // Sample position of field start
    bool isFirstField;      // True for odd field (field 1), false for even (field 2)
    size_t vsyncStart;      // First VSYNC pulse position
    size_t vsyncEnd;        // Last VSYNC pulse position
    int confidence;         // Detection confidence (0-100)
    size_t lineOffset;      // Line number within the RF data
};

/**
 * @brief Configuration for vertical sync detection
 */
struct VSyncConfig {
    double sampleRateMHz;      // Input sample rate
    double linePeriodUS;       // Line period (64µs for PAL, 63.5µs for NTSC)
    double hsyncPulseUS;       // HSYNC pulse width (4.7µs)
    double eqPulseUS;          // Equalizing pulse width (2.35µs)
    double vsyncPulseUS;       // VSYNC pulse width (27.3µs for PAL)
    int numEqPulses;           // Number of equalizing pulses per section (5 for PAL)
    size_t fieldLines;         // Lines per field (312.5 for PAL, 262.5 for NTSC)
    float syncThreshold;       // Threshold for sync detection (fraction of signal range)
    
    // Default PAL configuration
    VSyncConfig()
        : sampleRateMHz(40.0)
        , linePeriodUS(64.0)
        , hsyncPulseUS(4.7)
        , eqPulseUS(2.35)
        , vsyncPulseUS(27.3)
        , numEqPulses(5)
        , fieldLines(312)
        , syncThreshold(0.5f)
    {}
    
    // Helper to convert microseconds to samples
    size_t usToSamples(double us) const {
        return static_cast<size_t>(us * sampleRateMHz);
    }
};

/**
 * @brief Vertical sync detector
 * 
 * Detects field boundaries by analyzing the vertical blanking interval.
 * The VBI contains:
 * - Pre-equalizing pulses (2.5 lines)
 * - Broad (vertical sync) pulses (2.5 lines)
 * - Post-equalizing pulses (2.5 lines)
 * 
 * For PAL, the pattern is:
 *   5 pre-EQ pulses → 5 VSYNC pulses → 5 post-EQ pulses → regular HSYNC
 */
class VSyncDetector {
public:
    explicit VSyncDetector(const VSyncConfig& config);
    ~VSyncDetector();
    
    // Non-copyable
    VSyncDetector(const VSyncDetector&) = delete;
    VSyncDetector& operator=(const VSyncDetector&) = delete;
    
    // Movable
    VSyncDetector(VSyncDetector&&) noexcept;
    VSyncDetector& operator=(VSyncDetector&&) noexcept;
    
    /**
     * @brief Detect all sync pulses in the video data
     * @param video Demodulated video signal
     * @return Vector of detected pulses
     */
    std::vector<VPulse> detectPulses(const std::vector<float>& video);
    
    /**
     * @brief Find field boundaries in the pulse list
     * @param pulses List of detected pulses
     * @return Vector of field boundaries
     */
    std::vector<FieldBoundary> findFieldBoundaries(const std::vector<VPulse>& pulses);
    
    /**
     * @brief Complete detection: find all field boundaries in video data
     * @param video Demodulated video signal
     * @return Vector of field boundaries
     */
    std::vector<FieldBoundary> detect(const std::vector<float>& video);
    
    /**
     * @brief Find the first valid field start position
     * @param video Demodulated video signal
     * @param startOffset Start searching from this position
     * @return Position of first field, or -1 if not found
     */
    int64_t findFirstFieldStart(const std::vector<float>& video, size_t startOffset = 0);
    
    /**
     * @brief Classify a pulse based on its length
     * @param lengthSamples Pulse length in samples
     * @return Classified pulse type
     */
    VPulseType classifyPulse(size_t lengthSamples) const;
    
    /**
     * @brief Get expected samples per line
     */
    size_t getSamplesPerLine() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    VSyncConfig config_;
};

} // namespace sync
} // namespace vhsdecode
