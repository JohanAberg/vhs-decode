#include "vhsdecode/tbc_corrector.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>

namespace vhsdecode {
namespace video {

TBCCorrector::TBCCorrector(TVSystem system, double sampleRate)
    : system_(system)
    , sampleRate_(sampleRate)
    , hsyncThreshold_(-0.3)
{
    // Set system-specific parameters
    if (system == TVSystem::NTSC) {
        linesPerField_ = 262;  // NTSC: 262/263 alternating
        standardLineLength_ = 910;  // samples per line at 40 MSPS
    } else {  // PAL
        linesPerField_ = 312;  // PAL: 312/313 alternating
        standardLineLength_ = 1135;  // samples per line at 40 MSPS
    }
    
    std::cout << "✓ TBC corrector initialized" << std::endl;
    std::cout << "  System: " << (system == TVSystem::NTSC ? "NTSC" : "PAL") << std::endl;
    std::cout << "  Lines per field: " << linesPerField_ << std::endl;
    std::cout << "  Standard line length: " << standardLineLength_ << " samples" << std::endl;
}

std::vector<size_t>
TBCCorrector::detectHsyncs(const std::vector<double>& video) {
    std::vector<size_t> hsyncPositions;
    
    // Simple hsync detection: look for negative-going edges below threshold
    bool inSync = false;
    for (size_t i = 1; i < video.size(); ++i) {
        if (video[i] < hsyncThreshold_ && video[i-1] >= hsyncThreshold_) {
            if (!inSync) {
                hsyncPositions.push_back(i);
                inSync = true;
            }
        } else if (video[i] >= hsyncThreshold_) {
            inSync = false;
        }
    }
    
    return hsyncPositions;
}

std::vector<double>
TBCCorrector::normalizeLine(const std::vector<double>& line, size_t targetLength) {
    if (line.size() == targetLength) {
        return line;
    }
    
    // Simple linear interpolation for resampling
    std::vector<double> normalized(targetLength);
    double ratio = static_cast<double>(line.size() - 1) / static_cast<double>(targetLength - 1);
    
    for (size_t i = 0; i < targetLength; ++i) {
        double pos = i * ratio;
        size_t idx = static_cast<size_t>(pos);
        double frac = pos - idx;
        
        if (idx + 1 < line.size()) {
            normalized[i] = line[idx] * (1.0 - frac) + line[idx + 1] * frac;
        } else {
            normalized[i] = line[idx];
        }
    }
    
    return normalized;
}

TBCCorrector::TBCResult
TBCCorrector::correctAndAssemble(const std::vector<double>& video) {
    TBCResult result;
    
    // Detect all hsync positions
    auto hsyncPositions = detectHsyncs(video);
    
    if (hsyncPositions.empty()) {
        std::cerr << "Warning: No hsync pulses detected" << std::endl;
        return result;
    }
    
    // Extract and normalize lines
    std::vector<std::vector<double>> lines;
    for (size_t i = 0; i < hsyncPositions.size() - 1; ++i) {
        size_t lineStart = hsyncPositions[i];
        size_t lineEnd = hsyncPositions[i + 1];
        
        std::vector<double> line(video.begin() + lineStart, video.begin() + lineEnd);
        auto normalizedLine = normalizeLine(line, standardLineLength_);
        lines.push_back(normalizedLine);
    }
    
    // Assemble lines into fields
    std::vector<double> currentField;
    for (size_t i = 0; i < lines.size(); ++i) {
        // Append line to current field
        currentField.insert(currentField.end(), lines[i].begin(), lines[i].end());
        
        // Check if field is complete
        if ((i + 1) % linesPerField_ == 0) {
            result.fields.push_back(currentField);
            result.linesPerField.push_back(linesPerField_);
            currentField.clear();
        }
    }
    
    // Add partial field if any
    if (!currentField.empty()) {
        result.fields.push_back(currentField);
        result.linesPerField.push_back(currentField.size() / standardLineLength_);
    }
    
    result.totalLines = lines.size();
    
    std::cout << "✓ TBC correction complete" << std::endl;
    std::cout << "  Fields assembled: " << result.fields.size() << std::endl;
    std::cout << "  Lines processed: " << result.totalLines << std::endl;
    std::cout << "  Hsync detections: " << hsyncPositions.size() << std::endl;
    
    return result;
}

} // namespace video
} // namespace vhsdecode
