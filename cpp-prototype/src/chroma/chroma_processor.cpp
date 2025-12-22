#define _USE_MATH_DEFINES
#include "vhsdecode/chroma_processor.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vhsdecode {
namespace chroma {

ChromaProcessor::ChromaProcessor(const Config& config)
    : config_(config)
{
    if (!config_.filterSOS.empty()) {
        bandpassFilter_ = std::make_unique<filter::SOSFilter>(config_.filterSOS);
    }
}

ChromaProcessor::~ChromaProcessor() = default;

std::vector<RealArray> ChromaProcessor::processField(const std::vector<RealArray>& tbcChroma, int fieldNumber, int trackPhase) {
    std::vector<RealArray> processed = tbcChroma;

    int phaseRotation = 0;
    if (trackPhase >= 0) {
        const int parity = fieldNumber % 2;
        if (parity == trackPhase) {
            phaseRotation = config_.chromaRotation[0];
        } else {
            phaseRotation = config_.chromaRotation[1];
        }
    }

    applyHeterodyne(processed, phaseRotation);
    applyFilter(processed);
    applyComb(processed);
    applyAcc(processed);

    return processed;
}

void ChromaProcessor::applyHeterodyne(std::vector<RealArray>& field, int phaseRotation) {
    if (field.empty()) {
        return;
    }

    int phaseIdx = config_.startingPhase;
    const double omega = 2.0 * M_PI * config_.heterodyneFreq / config_.sampleRate;
    std::size_t sampleIndex = 0;

    for (auto& line : field) {
        const double phaseOffset = phaseIdx * (M_PI / 2.0);

        for (float& sample : line) {
            const double angle = omega * static_cast<double>(sampleIndex) + phaseOffset + config_.phaseOffset;
            sample = static_cast<float>(sample * (-std::cos(angle)));
            ++sampleIndex;
        }

        phaseIdx = (phaseIdx + phaseRotation) % 4;
        if (phaseIdx < 0) {
            phaseIdx += 4;
        }
    }
}

void ChromaProcessor::applyFilter(std::vector<RealArray>& field) {
    if (!bandpassFilter_) {
        return;
    }

    for (auto& line : field) {
        if (line.empty()) {
            continue;
        }

        std::vector<double> dLine(line.begin(), line.end());

        bandpassFilter_->reset();
        std::vector<double> forward = bandpassFilter_->filter(dLine);

        std::reverse(forward.begin(), forward.end());

        bandpassFilter_->reset();
        std::vector<double> backward = bandpassFilter_->filter(forward);

        std::reverse(backward.begin(), backward.end());

        for (std::size_t idx = 0; idx < line.size(); ++idx) {
            line[idx] = static_cast<float>(backward[idx]);
        }
    }
}

void ChromaProcessor::applyComb(std::vector<RealArray>& field) const {
    if (!config_.enableComb || field.empty()) {
        return;
    }

    const int delay = std::max(1, config_.combDelay);
    const std::size_t totalLines = field.size();
    if (totalLines <= static_cast<std::size_t>(delay * 2)) {
        return;
    }

    const int startLine = std::max(0, config_.combStartLine);
    const std::size_t beginLine = std::max<std::size_t>(static_cast<std::size_t>(startLine), static_cast<std::size_t>(delay));
    std::vector<RealArray> original = field;

    for (std::size_t lineIdx = beginLine;
        lineIdx + delay < totalLines;
         ++lineIdx) {
        const RealArray& advanced = original[lineIdx + delay];
        const RealArray& delayed = original[lineIdx - delay];
        const RealArray& currentOriginal = original[lineIdx];
        RealArray& current = field[lineIdx];

        const std::size_t length = current.size();
        for (std::size_t sampleIdx = 0; sampleIdx < length; ++sampleIdx) {
            const float value = ((currentOriginal[sampleIdx] * 2.0f) - delayed[sampleIdx] - advanced[sampleIdx]) * 0.25f;
            current[sampleIdx] = value;
        }
    }
}

void ChromaProcessor::applyAcc(std::vector<RealArray>& field) const {
    if (field.empty()) {
        return;
    }

    if (config_.burstAbsRef <= 0.0) {
        return;
    }

    const int lineCount = static_cast<int>(field.size());
    const int startLine = std::clamp(config_.accStartLine, 0, lineCount);
    const int burstStart = std::clamp(config_.burstStartSample, 0, static_cast<int>(config_.lineLength));
    const int burstEnd = std::clamp(config_.burstEndSample, burstStart, static_cast<int>(config_.lineLength));

    for (int lineIdx = startLine; lineIdx < lineCount; ++lineIdx) {
        RealArray& line = field[static_cast<std::size_t>(lineIdx)];
        const int burstRange = burstEnd - burstStart;
        double sumSquares = 0.0;
        for (int idx = burstStart; idx < burstEnd; ++idx) {
            const double value = static_cast<double>(line[static_cast<std::size_t>(idx)]);
            sumSquares += value * value;
        }
        const double burstRms = (burstRange > 0) ? std::sqrt(sumSquares / static_cast<double>(burstRange)) : 0.0;
        
        const double scale = (burstRms > 0.0) ? config_.burstAbsRef / burstRms : 1.0;

        for (float& sample : line) {
            sample = static_cast<float>(sample * scale);
        }
    }
}

double ChromaProcessor::computeRMS(const RealArray& line, int startSample, int endSample) {
    if (line.empty() || endSample <= startSample) {
        return 0.0;
    }

    const int start = std::clamp(startSample, 0, static_cast<int>(line.size()));
    const int end = std::clamp(endSample, start, static_cast<int>(line.size()));
    if (end <= start) {
        return 0.0;
    }

    double sumSquares = 0.0;
    for (int idx = start; idx < end; ++idx) {
        const double value = static_cast<double>(line[static_cast<std::size_t>(idx)]);
        sumSquares += value * value;
    }

    const double sampleCount = static_cast<double>(end - start);
    return sampleCount > 0.0 ? std::sqrt(sumSquares / sampleCount) : 0.0;
}

} // namespace chroma
} // namespace vhsdecode
