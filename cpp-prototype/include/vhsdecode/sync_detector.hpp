#ifndef VHSDECODE_SYNC_DETECTOR_HPP
#define VHSDECODE_SYNC_DETECTOR_HPP

#include "vhsdecode/types.hpp"
#include <vector>
#include <map>
#include <cstdint>

namespace vhsdecode {
namespace sync {

/**
 * Represents a detected sync pulse
 */
struct Pulse {
    size_t start;    // Sample position where pulse starts
    size_t length;   // Length of pulse in samples
    
    Pulse(size_t s, size_t l) : start(s), length(l) {}
};

/**
 * Line location information
 */
struct LineInfo {
    size_t lineNumber;   // Line number in field
    double startSample;  // Fractional sample position of line start
    double length;       // Length of line in samples
    bool valid;          // Whether this line has valid sync
    
    LineInfo() : lineNumber(0), startSample(0), length(0), valid(false) {}
    LineInfo(size_t num, double start, double len, bool v = true)
        : lineNumber(num), startSample(start), length(len), valid(v) {}
};

/**
 * Sync detection configuration
 */
struct SyncConfig {
    // PAL parameters (default)
    double lineFreqHz = 15625.0;        // Line frequency (PAL: 15625 Hz, NTSC: 15734.26 Hz)
    double sampleRateMHz = 40.0;        // Sample rate in MHz
    int linesPerField = 312;            // Lines per field (PAL: 312/313, NTSC: 262/263)
    
    // Sync pulse parameters
    double hsyncPulseUS = 4.7;          // Horizontal sync pulse width in microseconds
    double vsyncPulseUS = 27.3;         // Vertical sync pulse width in microseconds
    
    // IRE levels for sync detection
    double syncTipIRE = -40.0;          // Sync tip level
    double blackLevelIRE = 0.0;         // Black level
    double blankingLevelIRE = 0.0;      // Blanking level
    
    // Derived values (computed from above)
    double samplesPerLine() const {
        return sampleRateMHz * 1e6 / lineFreqHz;
    }
    
    double hsyncSamples() const {
        return hsyncPulseUS * sampleRateMHz;
    }
    
    double vsyncSamples() const {
        return vsyncPulseUS * sampleRateMHz;
    }
};

/**
 * Sync detector class - finds horizontal and vertical sync pulses
 * and aligns video lines
 */
class SyncDetector {
public:
    explicit SyncDetector(const SyncConfig& config);
    ~SyncDetector();
    
    // Non-copyable
    SyncDetector(const SyncDetector&) = delete;
    SyncDetector& operator=(const SyncDetector&) = delete;
    
    // Movable
    SyncDetector(SyncDetector&&) noexcept;
    SyncDetector& operator=(SyncDetector&&) noexcept;
    
    /**
     * Find all sync pulses in the demodulated video signal
     * @param video Demodulated video signal (frequency in Hz)
     * @param syncThreshold Threshold for sync detection (frequency in Hz)
     * @return Vector of detected pulses
     */
    std::vector<Pulse> findPulses(const RealArray& video, float syncThreshold);
    
    /**
     * Compute line locations from detected pulses
     * @param pulses Vector of detected sync pulses
     * @param expectedLineLength Expected line length in samples
     * @return Vector of line information
     */
    std::vector<LineInfo> computeLineLocations(
        const std::vector<Pulse>& pulses,
        double expectedLineLength,
        const RealArray& video,
        float syncThreshold);
    
    /**
     * Compute line locations using Python's algorithm:
     * Assigns pulses to line numbers, keeps actual positions, fills gaps
     * @param pulses Vector of detected sync pulses
     * @param line0loc Sample position of line 0 (field start)
     * @param meanLineLength Mean line length in samples
     * @param numLines Number of lines in field
     * @param video Video signal for refinement
     * @param syncThreshold Sync detection threshold
     * @return Map of line number to actual pulse position (with gaps filled)
     */
    std::map<int, double> computeLineLocsDict(
        const std::vector<Pulse>& pulses,
        double line0loc,
        double meanLineLength,
        int numLines,
        const RealArray& video,
        float syncThreshold);
    
    /**
     * Find the start of horizontal sync using zero-crossing detection
     * @param video Video signal
     * @param startPos Starting position to search from
     * @param threshold Sync detection threshold
     * @param searchRange Number of samples to search
     * @return Fractional sample position of sync start, or -1 if not found
     */
    double findSyncEdge(const RealArray& video, size_t startPos, 
                        float threshold, size_t searchRange);
    
    /**
     * Refine sync position using interpolation
     * @param video Video signal
     * @param roughPos Rough sync position
     * @param threshold Sync detection threshold
     * @return Refined fractional sample position
     */
    double refineSyncPosition(const RealArray& video, size_t roughPos,
                              float threshold);
    
    /**
     * Extract aligned video lines from raw demodulated data
     * @param video Raw demodulated video
     * @param lineLocations Line location information
     * @param outputLineLength Desired output line length in samples
     * @return Aligned video data (one line per row)
     */
    std::vector<RealArray> extractAlignedLines(
        const RealArray& video,
        const std::vector<LineInfo>& lineLocations,
        size_t outputLineLength);
    
    /**
     * Get sync configuration
     */
    const SyncConfig& getConfig() const { return config_; }

private:
    SyncConfig config_;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sync
} // namespace vhsdecode

#endif // VHSDECODE_SYNC_DETECTOR_HPP
