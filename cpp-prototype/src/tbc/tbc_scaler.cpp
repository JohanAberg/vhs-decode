/**
 * TBC Scaler - Time-Base Correction scaling
 * 
 * Resamples video lines from input sample rate to output sample rate
 * using bicubic interpolation (matching Python's scale() function).
 */

#include "vhsdecode/tbc_scaler.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace vhsdecode {
namespace tbc {

TBCScaler::TBCScaler(const Config& config)
    : config_(config)
{
}

TBCScaler::~TBCScaler() = default;

TBCScaler::TBCScaler(TBCScaler&&) noexcept = default;
TBCScaler& TBCScaler::operator=(TBCScaler&&) noexcept = default;

float TBCScaler::bicubicInterpolate(float p0, float p1, float p2, float p3, float x) {
    // Bicubic interpolation formula from https://www.paulinternet.nl/?page=bicubic
    // Matches Python's scale() function exactly
    return p1 + 0.5f * x * (
        p2 - p0 + x * (
            2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + x * (
                3.0f * (p1 - p2) + p3 - p0
            )
        )
    );
}

RealArray TBCScaler::scaleLine(const RealArray& input, double begin, double end) {
    double linelen = end - begin;
    double sfactor = linelen / config_.outputLineLength;
    
    RealArray output(config_.outputLineLength);
    
    for (int i = 0; i < config_.outputLineLength; ++i) {
        double coord = (i * sfactor) + begin;
        int start = static_cast<int>(coord) - 1;
        
        // Clamp indices to valid range
        int idx0 = std::max(0, std::min(start, static_cast<int>(input.size()) - 1));
        int idx1 = std::max(0, std::min(start + 1, static_cast<int>(input.size()) - 1));
        int idx2 = std::max(0, std::min(start + 2, static_cast<int>(input.size()) - 1));
        int idx3 = std::max(0, std::min(start + 3, static_cast<int>(input.size()) - 1));
        
        float p0 = input[idx0];
        float p1 = input[idx1];
        float p2 = input[idx2];
        float p3 = input[idx3];
        
        float x = static_cast<float>(coord - static_cast<int>(coord));
        
        output[i] = bicubicInterpolate(p0, p1, p2, p3, x);
    }
    
    return output;
}

RealArray TBCScaler::scaleLine(const RealArray& input, size_t lineStart, size_t lineLength) {
    return scaleLine(input, static_cast<double>(lineStart), 
                     static_cast<double>(lineStart + lineLength));
}

std::vector<RealArray> TBCScaler::scaleField(
    const RealArray& input,
    const std::vector<size_t>& lineStarts,
    size_t numLines)
{
    std::vector<RealArray> output;
    output.reserve(numLines);
    
    int inputLineLen = config_.inputLineLength();
    
    for (size_t line = 0; line < numLines; ++line) {
        double begin, end;
        
        if (line < lineStarts.size()) {
            begin = static_cast<double>(lineStarts[line]);
            
            // End is either next line start or begin + nominal line length
            if (line + 1 < lineStarts.size()) {
                end = static_cast<double>(lineStarts[line + 1]);
            } else {
                end = begin + inputLineLen;
            }
        } else {
            // No line start info - use fixed positions
            begin = static_cast<double>(line * inputLineLen);
            end = begin + inputLineLen;
        }
        
        // Safety check
        if (begin >= input.size() || end > input.size()) {
            // Fill with blanking level
            RealArray blankLine(config_.outputLineLength, 16384.0f);
            output.push_back(blankLine);
        } else {
            output.push_back(scaleLine(input, begin, end));
        }
    }
    
    return output;
}

} // namespace tbc
} // namespace vhsdecode
