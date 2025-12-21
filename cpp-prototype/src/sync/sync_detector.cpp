#include "vhsdecode/sync_detector.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace vhsdecode {
namespace sync {

class SyncDetector::Impl {
public:
    SyncConfig config;
    
    explicit Impl(const SyncConfig& cfg) : config(cfg) {}
};

SyncDetector::SyncDetector(const SyncConfig& config)
    : config_(config)
    , impl_(std::make_unique<Impl>(config))
{
}

SyncDetector::~SyncDetector() = default;

SyncDetector::SyncDetector(SyncDetector&&) noexcept = default;
SyncDetector& SyncDetector::operator=(SyncDetector&&) noexcept = default;

std::vector<Pulse> SyncDetector::findPulses(const RealArray& video, float syncThreshold) {
    std::vector<Pulse> pulses;
    
    if (video.empty()) {
        return pulses;
    }
    
    // Minimum sync pulse length (about 2 microseconds at sample rate)
    size_t minPulseLen = static_cast<size_t>(2.0 * config_.sampleRateMHz);
    // Maximum sync pulse length (about 30 microseconds - longer than vsync)
    size_t maxPulseLen = static_cast<size_t>(30.0 * config_.sampleRateMHz);
    
    bool inPulse = video[0] <= syncThreshold;
    size_t pulseStart = 0;
    
    for (size_t i = 1; i < video.size(); ++i) {
        if (inPulse) {
            // Currently in a pulse, check if we exit
            if (video[i] > syncThreshold) {
                size_t pulseLen = i - pulseStart;
                
                // Only keep pulses within expected length range
                if (pulseLen >= minPulseLen && pulseLen <= maxPulseLen && pulseStart > 0) {
                    pulses.emplace_back(pulseStart, pulseLen);
                }
                inPulse = false;
            }
        } else {
            // Not in pulse, check if we enter
            if (video[i] <= syncThreshold) {
                pulseStart = i;
                inPulse = true;
            }
        }
    }
    
    return pulses;
}

std::vector<LineInfo> SyncDetector::computeLineLocations(
    const std::vector<Pulse>& pulses,
    double expectedLineLength) 
{
    std::vector<LineInfo> lineLocations;
    
    if (pulses.empty()) {
        return lineLocations;
    }
    
    // Tolerance for line length variation (VHS can have significant wow/flutter)
    double minLineLen = expectedLineLength * 0.9;
    double maxLineLen = expectedLineLength * 1.1;
    
    // Expected hsync pulse length
    double expectedHsyncLen = config_.hsyncSamples();
    double minHsyncLen = expectedHsyncLen * 0.5;
    double maxHsyncLen = expectedHsyncLen * 2.0;
    
    // Filter pulses to likely horizontal syncs (not vsync or eq pulses)
    std::vector<const Pulse*> hsyncCandidates;
    for (const auto& pulse : pulses) {
        if (pulse.length >= minHsyncLen && pulse.length <= maxHsyncLen) {
            hsyncCandidates.push_back(&pulse);
        }
    }
    
    if (hsyncCandidates.size() < 2) {
        return lineLocations;
    }
    
    // Compute line lengths between consecutive hsync pulses
    std::vector<double> lineLengths;
    for (size_t i = 1; i < hsyncCandidates.size(); ++i) {
        double len = static_cast<double>(hsyncCandidates[i]->start - hsyncCandidates[i-1]->start);
        if (len >= minLineLen && len <= maxLineLen) {
            lineLengths.push_back(len);
        }
    }
    
    if (lineLengths.empty()) {
        return lineLocations;
    }
    
    // Compute median line length
    std::vector<double> sortedLengths = lineLengths;
    std::sort(sortedLengths.begin(), sortedLengths.end());
    double medianLineLen = sortedLengths[sortedLengths.size() / 2];
    
    // Build line locations using detected syncs
    size_t lineNum = 0;
    for (size_t i = 0; i < hsyncCandidates.size() - 1; ++i) {
        double startPos = static_cast<double>(hsyncCandidates[i]->start);
        double lineLen = static_cast<double>(hsyncCandidates[i+1]->start - hsyncCandidates[i]->start);
        
        // Check if this is a valid line length
        bool valid = (lineLen >= minLineLen && lineLen <= maxLineLen);
        
        lineLocations.emplace_back(lineNum, startPos, lineLen, valid);
        lineNum++;
    }
    
    // Add last line with estimated length
    if (!hsyncCandidates.empty()) {
        double lastStart = static_cast<double>(hsyncCandidates.back()->start);
        lineLocations.emplace_back(lineNum, lastStart, medianLineLen, true);
    }
    
    return lineLocations;
}

double SyncDetector::findSyncEdge(const RealArray& video, size_t startPos,
                                   float threshold, size_t searchRange) 
{
    if (startPos >= video.size()) {
        return -1.0;
    }
    
    size_t endPos = std::min(startPos + searchRange, video.size());
    
    // Look for falling edge (entering sync)
    for (size_t i = startPos + 1; i < endPos; ++i) {
        if (video[i-1] > threshold && video[i] <= threshold) {
            // Found falling edge, interpolate exact crossing
            float a = video[i-1] - threshold;
            float b = video[i] - threshold;
            if (b - a != 0) {
                double frac = -a / (b - a);
                return static_cast<double>(i - 1) + frac;
            }
            return static_cast<double>(i);
        }
    }
    
    return -1.0;
}

double SyncDetector::refineSyncPosition(const RealArray& video, size_t roughPos,
                                         float threshold) 
{
    if (roughPos >= video.size() || roughPos < 1) {
        return static_cast<double>(roughPos);
    }
    
    // Look in a small window around the rough position
    size_t windowStart = (roughPos > 10) ? roughPos - 10 : 0;
    size_t windowEnd = std::min(roughPos + 10, video.size());
    
    // Find the exact crossing point
    for (size_t i = windowStart + 1; i < windowEnd; ++i) {
        if (video[i-1] > threshold && video[i] <= threshold) {
            // Interpolate
            float a = video[i-1] - threshold;
            float b = video[i] - threshold;
            if (b - a != 0) {
                double frac = -a / (b - a);
                return static_cast<double>(i - 1) + frac;
            }
            return static_cast<double>(i);
        }
    }
    
    return static_cast<double>(roughPos);
}

std::vector<RealArray> SyncDetector::extractAlignedLines(
    const RealArray& video,
    const std::vector<LineInfo>& lineLocations,
    size_t outputLineLength)
{
    std::vector<RealArray> alignedLines;
    
    for (const auto& line : lineLocations) {
        if (!line.valid) {
            // For invalid lines, output blank/gray
            RealArray blankLine(outputLineLength, 0.0f);
            alignedLines.push_back(std::move(blankLine));
            continue;
        }
        
        RealArray alignedLine(outputLineLength);
        size_t srcStart = static_cast<size_t>(line.startSample);
        
        // Simple nearest-neighbor resampling for now
        // TODO: Implement proper interpolation for time-base correction
        double scale = line.length / static_cast<double>(outputLineLength);
        
        for (size_t i = 0; i < outputLineLength; ++i) {
            size_t srcPos = srcStart + static_cast<size_t>(i * scale);
            if (srcPos < video.size()) {
                alignedLine[i] = video[srcPos];
            } else {
                alignedLine[i] = 0.0f;  // Beyond end of data
            }
        }
        
        alignedLines.push_back(std::move(alignedLine));
    }
    
    return alignedLines;
}

} // namespace sync
} // namespace vhsdecode
