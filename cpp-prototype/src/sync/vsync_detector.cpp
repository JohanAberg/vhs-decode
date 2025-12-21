#include "vhsdecode/vsync_detector.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <memory>

namespace vhsdecode {
namespace sync {

// Implementation class
class VSyncDetector::Impl {
public:
    explicit Impl(const VSyncConfig& config)
        : config_(config)
        , samplesPerLine_(config.usToSamples(config.linePeriodUS))
        , hsyncMinLen_(static_cast<size_t>(config.usToSamples(config.hsyncPulseUS) * 0.6))
        , hsyncMaxLen_(static_cast<size_t>(config.usToSamples(config.hsyncPulseUS) * 1.6))
        , eqMinLen_(static_cast<size_t>(config.usToSamples(config.eqPulseUS) * 0.6))
        , eqMaxLen_(static_cast<size_t>(config.usToSamples(config.eqPulseUS) * 1.6))
        , vsyncMinLen_(static_cast<size_t>(config.usToSamples(config.vsyncPulseUS) * 0.6))
        , vsyncMaxLen_(static_cast<size_t>(config.usToSamples(config.vsyncPulseUS) * 1.6))
    {
        // Log computed thresholds
        std::cout << "[VSyncDetector] Pulse length thresholds (samples at " 
                  << config_.sampleRateMHz << " MHz):" << std::endl;
        std::cout << "  HSYNC: " << hsyncMinLen_ << " - " << hsyncMaxLen_ 
                  << " (nominal " << config.usToSamples(config.hsyncPulseUS) << ")" << std::endl;
        std::cout << "  EQ: " << eqMinLen_ << " - " << eqMaxLen_
                  << " (nominal " << config.usToSamples(config.eqPulseUS) << ")" << std::endl;
        std::cout << "  VSYNC: " << vsyncMinLen_ << " - " << vsyncMaxLen_
                  << " (nominal " << config.usToSamples(config.vsyncPulseUS) << ")" << std::endl;
        std::cout << "  Samples per line: " << samplesPerLine_ << std::endl;
    }
    
    VPulseType classifyPulse(size_t lengthSamples) const {
        // Check from shortest to longest
        if (lengthSamples >= eqMinLen_ && lengthSamples <= eqMaxLen_) {
            return VPulseType::EQUALIZING;
        }
        if (lengthSamples >= hsyncMinLen_ && lengthSamples <= hsyncMaxLen_) {
            return VPulseType::HSYNC;
        }
        if (lengthSamples >= vsyncMinLen_ && lengthSamples <= vsyncMaxLen_) {
            return VPulseType::VSYNC;
        }
        return VPulseType::UNKNOWN;
    }
    
    std::vector<VPulse> detectPulses(const std::vector<float>& video) {
        std::vector<VPulse> pulses;
        
        if (video.size() < samplesPerLine_ * 10) {
            std::cerr << "[VSyncDetector] Video too short for pulse detection" << std::endl;
            return pulses;
        }
        
        // Find signal range for threshold calculation
        // Sample every 1000th value for speed
        float minVal = video[0], maxVal = video[0];
        for (size_t i = 0; i < video.size(); i += 1000) {
            if (video[i] < minVal) minVal = video[i];
            if (video[i] > maxVal) maxVal = video[i];
        }
        
        // Sync tips are at the bottom of the signal
        // Threshold is partway between min and max
        float range = maxVal - minVal;
        float threshold = minVal + range * config_.syncThreshold;
        
        std::cout << "[VSyncDetector] Signal range: " << minVal << " to " << maxVal 
                  << ", threshold: " << threshold << std::endl;
        
        // State machine for pulse detection
        bool inPulse = false;
        size_t pulseStart = 0;
        float pulseMin = 0;
        
        // Detect pulses by finding where signal goes below threshold
        for (size_t i = 1; i < video.size(); ++i) {
            bool belowThreshold = video[i] < threshold;
            
            if (!inPulse && belowThreshold) {
                // Start of pulse
                inPulse = true;
                pulseStart = i;
                pulseMin = video[i];
            } else if (inPulse && belowThreshold) {
                // Still in pulse
                if (video[i] < pulseMin) {
                    pulseMin = video[i];
                }
            } else if (inPulse && !belowThreshold) {
                // End of pulse
                inPulse = false;
                size_t pulseEnd = i;
                size_t pulseLen = pulseEnd - pulseStart;
                
                // Only record pulses longer than minimum EQ pulse
                if (pulseLen >= eqMinLen_ / 2) {
                    VPulse p;
                    p.start = pulseStart;
                    p.end = pulseEnd;
                    p.length = pulseLen;
                    p.type = classifyPulse(pulseLen);
                    p.amplitude = minVal - pulseMin; // Depth of sync
                    pulses.push_back(p);
                }
            }
        }
        
        std::cout << "[VSyncDetector] Detected " << pulses.size() << " pulses" << std::endl;
        
        // Count pulse types
        size_t hsyncCount = 0, eqCount = 0, vsyncCount = 0, unknownCount = 0;
        for (const auto& p : pulses) {
            switch (p.type) {
                case VPulseType::HSYNC: hsyncCount++; break;
                case VPulseType::EQUALIZING: eqCount++; break;
                case VPulseType::VSYNC: vsyncCount++; break;
                default: unknownCount++; break;
            }
        }
        std::cout << "[VSyncDetector] Pulse classification: HSYNC=" << hsyncCount 
                  << " EQ=" << eqCount << " VSYNC=" << vsyncCount 
                  << " UNKNOWN=" << unknownCount << std::endl;
        
        return pulses;
    }
    
    /**
     * @brief State machine for VBI detection (matching Python's run_vblank_state_machine)
     * 
     * States:
     *   -1 (HSYNC_SEARCH): Looking for transition to equalizing pulses
     *    0 (EQPL1): First equalizing pulse section
     *    1 (VSYNC): Broad vertical sync pulses  
     *    2 (EQPL2): Second equalizing pulse section
     *    3 (DONE): Found complete VBI, back to regular HSYNC
     */
    std::vector<FieldBoundary> findFieldBoundaries(const std::vector<VPulse>& pulses) {
        std::vector<FieldBoundary> boundaries;
        
        if (pulses.size() < 20) {
            std::cerr << "[VSyncDetector] Not enough pulses for field boundary detection" << std::endl;
            return boundaries;
        }
        
        const int STATE_HSYNC_SEARCH = -1;
        const int STATE_EQPL1 = 0;
        const int STATE_VSYNC = 1;
        const int STATE_EQPL2 = 2;
        const int STATE_DONE = 3;
        
        int state = STATE_HSYNC_SEARCH;
        int pulseCount = 0;
        size_t vsyncStartIdx = 0;
        size_t vsyncEndIdx = 0;
        size_t eqpl1StartIdx = 0;
        
        for (size_t i = 0; i < pulses.size(); ++i) {
            const VPulse& p = pulses[i];
            
            if (state == STATE_HSYNC_SEARCH) {
                // Looking for first equalizing pulse (transition from HSYNC to EQ)
                if (p.type == VPulseType::EQUALIZING) {
                    state = STATE_EQPL1;
                    pulseCount = 1;
                    eqpl1StartIdx = i;
                } else if (p.type == VPulseType::HSYNC) {
                    // Normal HSYNC, keep searching
                    pulseCount++;
                }
            } 
            else if (state == STATE_EQPL1) {
                // In first equalizing pulse section
                if (p.type == VPulseType::EQUALIZING) {
                    pulseCount++;
                    if (pulseCount >= config_.numEqPulses) {
                        // Got enough EQ pulses, expect VSYNC next
                        state = STATE_VSYNC;
                        pulseCount = 0;
                    }
                } else if (p.type == VPulseType::VSYNC) {
                    // Transition to VSYNC (might have fewer EQ pulses)
                    state = STATE_VSYNC;
                    pulseCount = 1;
                    vsyncStartIdx = i;
                } else if (p.type == VPulseType::HSYNC) {
                    // False alarm, back to searching
                    state = STATE_HSYNC_SEARCH;
                    pulseCount = 0;
                }
            }
            else if (state == STATE_VSYNC) {
                // In vertical sync section
                if (p.type == VPulseType::VSYNC) {
                    if (pulseCount == 0) {
                        vsyncStartIdx = i;
                    }
                    pulseCount++;
                    vsyncEndIdx = i;
                } else if (p.type == VPulseType::EQUALIZING) {
                    // Transition to post-equalizing
                    if (pulseCount >= 4) {  // Need at least 4 VSYNC pulses
                        state = STATE_EQPL2;
                        pulseCount = 1;
                    } else {
                        // Not enough VSYNC, probably false detect
                        state = STATE_HSYNC_SEARCH;
                        pulseCount = 0;
                    }
                } else if (p.type == VPulseType::HSYNC) {
                    // Unexpected HSYNC during VSYNC
                    state = STATE_HSYNC_SEARCH;
                    pulseCount = 0;
                }
            }
            else if (state == STATE_EQPL2) {
                // In second equalizing pulse section
                if (p.type == VPulseType::EQUALIZING) {
                    pulseCount++;
                } else if (p.type == VPulseType::HSYNC) {
                    // Found end of VBI!
                    if (pulseCount >= 3) {  // Need at least 3 post-EQ pulses
                        state = STATE_DONE;
                    } else {
                        // Not enough EQ pulses
                        state = STATE_HSYNC_SEARCH;
                        pulseCount = 0;
                    }
                } else if (p.type == VPulseType::VSYNC) {
                    // Unexpected
                    state = STATE_HSYNC_SEARCH;
                    pulseCount = 0;
                }
            }
            
            if (state == STATE_DONE) {
                // Found a complete VBI sequence
                FieldBoundary fb;
                
                // Field starts at the first VSYNC pulse (or EQ pulses before it)
                fb.vsyncStart = pulses[vsyncStartIdx].start;
                fb.vsyncEnd = pulses[vsyncEndIdx].end;
                
                // Field boundary is traditionally at the start of the first broad pulse
                // For line numbering, line 1 starts at the first HSYNC after VBI
                fb.position = pulses[i].start;  // First HSYNC after VBI
                
                // Determine if odd or even field based on position of EQ pulses
                // Odd field: EQ pulses start at line 1, end at half-line
                // Even field: EQ pulses start at half-line
                // We can estimate by checking timing between EQ sections
                if (eqpl1StartIdx > 0) {
                    size_t eqStartPos = pulses[eqpl1StartIdx].start;
                    size_t prevPulsePos = pulses[eqpl1StartIdx - 1].start;
                    size_t gap = eqStartPos - prevPulsePos;
                    // If gap is ~half a line, this is an odd field
                    fb.isFirstField = (gap < samplesPerLine_ * 0.75);
                } else {
                    fb.isFirstField = true;  // Assume first field
                }
                
                fb.confidence = 80;  // Good detection
                fb.lineOffset = fb.position / samplesPerLine_;
                
                std::cout << "[VSyncDetector] Found field boundary at sample " << fb.position
                          << " (line ~" << fb.lineOffset << "), field " 
                          << (fb.isFirstField ? "1 (odd)" : "2 (even)") << std::endl;
                
                boundaries.push_back(fb);
                
                // Reset state machine to find next field
                state = STATE_HSYNC_SEARCH;
                pulseCount = 0;
            }
        }
        
        std::cout << "[VSyncDetector] Found " << boundaries.size() << " field boundaries" << std::endl;
        
        return boundaries;
    }
    
    size_t getSamplesPerLine() const { return samplesPerLine_; }
    
private:
    VSyncConfig config_;
    size_t samplesPerLine_;
    size_t hsyncMinLen_, hsyncMaxLen_;
    size_t eqMinLen_, eqMaxLen_;
    size_t vsyncMinLen_, vsyncMaxLen_;
};

// Main class implementation - delegates to Impl
VSyncDetector::VSyncDetector(const VSyncConfig& config)
    : impl_(std::make_unique<Impl>(config))
    , config_(config)
{}

VSyncDetector::~VSyncDetector() = default;

VSyncDetector::VSyncDetector(VSyncDetector&&) noexcept = default;
VSyncDetector& VSyncDetector::operator=(VSyncDetector&&) noexcept = default;

std::vector<VPulse> VSyncDetector::detectPulses(const std::vector<float>& video) {
    return impl_->detectPulses(video);
}

std::vector<FieldBoundary> VSyncDetector::findFieldBoundaries(const std::vector<VPulse>& pulses) {
    return impl_->findFieldBoundaries(pulses);
}

std::vector<FieldBoundary> VSyncDetector::detect(const std::vector<float>& video) {
    auto pulses = detectPulses(video);
    return findFieldBoundaries(pulses);
}

int64_t VSyncDetector::findFirstFieldStart(const std::vector<float>& video, size_t startOffset) {
    // Create a view starting at offset (by copying relevant portion)
    std::vector<float> subview;
    if (startOffset > 0 && startOffset < video.size()) {
        subview.assign(video.begin() + startOffset, video.end());
    } else {
        subview = video;
    }
    
    auto boundaries = detect(subview);
    if (!boundaries.empty()) {
        return static_cast<int64_t>(startOffset + boundaries[0].position);
    }
    return -1;
}

VPulseType VSyncDetector::classifyPulse(size_t lengthSamples) const {
    return impl_->classifyPulse(lengthSamples);
}

size_t VSyncDetector::getSamplesPerLine() const {
    return impl_->getSamplesPerLine();
}

} // namespace sync
} // namespace vhsdecode
