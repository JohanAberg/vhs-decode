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

    static bool loggedInputStats = false;
    if (!loggedInputStats && !processed.empty()) {
        const int lineIdx = std::min<int>(config_.accStartLine, static_cast<int>(processed.size()) - 1);
        const RealArray& line = processed[static_cast<std::size_t>(lineIdx)];
        const int burstStart = std::clamp(config_.burstStartSample, 0, static_cast<int>(line.size()));
        const int burstEnd = std::clamp(config_.burstEndSample, burstStart, static_cast<int>(line.size()));
        double sumSquares = 0.0;
        for (int idx = burstStart; idx < burstEnd; ++idx) {
            const double value = static_cast<double>(line[static_cast<std::size_t>(idx)]);
            sumSquares += value * value;
        }
        const int burstRange = std::max(1, burstEnd - burstStart);
        const double rms = std::sqrt(sumSquares / static_cast<double>(burstRange));
        std::cout << "[ChromaProcessor] Pre-heterodyne line " << lineIdx
                  << ": min=" << *std::min_element(line.begin(), line.end())
                  << " max=" << *std::max_element(line.begin(), line.end())
                  << " mean=" << (std::accumulate(line.begin(), line.end(), 0.0) / static_cast<double>(line.size()))
                  << " burstRMS=" << rms
                  << " firstSamples=";
        for (int idx = burstStart; idx < std::min(burstEnd, burstStart + 5); ++idx) {
            std::cout << ' ' << line[static_cast<std::size_t>(idx)];
        }
        std::cout << '\n';
        loggedInputStats = true;
    }

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
    static bool loggedPostHet = false;

    for (auto& line : field) {
        const double phaseOffset = phaseIdx * (M_PI / 2.0);

        for (float& sample : line) {
            const double angle = omega * static_cast<double>(sampleIndex) + phaseOffset;
            sample = static_cast<float>(sample * (-std::cos(angle)));
            ++sampleIndex;
        }

        phaseIdx = (phaseIdx + phaseRotation) % 4;
        if (phaseIdx < 0) {
            phaseIdx += 4;
        }

        if (!loggedPostHet) {
            const int burstStart = std::clamp(config_.burstStartSample, 0, static_cast<int>(line.size()));
            const int burstEnd = std::clamp(config_.burstEndSample, burstStart, static_cast<int>(line.size()));
            double sumSquares = 0.0;
            for (int idx = burstStart; idx < burstEnd; ++idx) {
                const double value = static_cast<double>(line[static_cast<std::size_t>(idx)]);
                sumSquares += value * value;
            }
            const int burstRange = std::max(1, burstEnd - burstStart);
            const double rms = std::sqrt(sumSquares / static_cast<double>(burstRange));
            std::cout << "[ChromaProcessor] Post-heterodyne line: min=" << *std::min_element(line.begin(), line.end())
                      << " max=" << *std::max_element(line.begin(), line.end())
                      << " mean=" << (std::accumulate(line.begin(), line.end(), 0.0) / static_cast<double>(line.size()))
                      << " burstRMS=" << rms
                      << " firstSamples=";
            for (int idx = burstStart; idx < std::min(burstEnd, burstStart + 5); ++idx) {
                std::cout << ' ' << line[static_cast<std::size_t>(idx)];
            }
            std::cout << '\n';
            loggedPostHet = true;
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

    static bool loggedPostFilter = false;
    if (!loggedPostFilter && !field.empty()) {
        const RealArray& line = field[0];
        const int burstStart = std::clamp(config_.burstStartSample, 0, static_cast<int>(line.size()));
        const int burstEnd = std::clamp(config_.burstEndSample, burstStart, static_cast<int>(line.size()));
        double sumSquares = 0.0;
        for (int idx = burstStart; idx < burstEnd; ++idx) {
            const double value = static_cast<double>(line[static_cast<std::size_t>(idx)]);
            sumSquares += value * value;
        }
        const int burstRange = std::max(1, burstEnd - burstStart);
        const double rms = std::sqrt(sumSquares / static_cast<double>(burstRange));
        std::cout << "[ChromaProcessor] Post-filter line: min=" << *std::min_element(line.begin(), line.end())
                  << " max=" << *std::max_element(line.begin(), line.end())
                  << " mean=" << (std::accumulate(line.begin(), line.end(), 0.0) / static_cast<double>(line.size()))
                  << " burstRMS=" << rms
                  << " firstSamples=";
        for (int idx = burstStart; idx < std::min(burstEnd, burstStart + 5); ++idx) {
            std::cout << ' ' << line[static_cast<std::size_t>(idx)];
        }
        std::cout << '\n';
        loggedPostFilter = true;
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

    static bool accWindowLogged = false;
    if (!accWindowLogged) {
        std::cout << "[ChromaProcessor] ACC window start=" << burstStart
                  << " end=" << burstEnd
                  << " length=" << (burstEnd - burstStart) << '\n';
        accWindowLogged = true;
    }

    for (int lineIdx = startLine; lineIdx < lineCount; ++lineIdx) {
        RealArray& line = field[static_cast<std::size_t>(lineIdx)];
        const int burstRange = burstEnd - burstStart;
        double sumSquares = 0.0;
        for (int idx = burstStart; idx < burstEnd; ++idx) {
            const double value = static_cast<double>(line[static_cast<std::size_t>(idx)]);
            sumSquares += value * value;
        }
        const double burstRms = (burstRange > 0) ? std::sqrt(sumSquares / static_cast<double>(burstRange)) : 0.0;
        float maxBurst = 0.0f;
        int maxBurstIdx = burstStart;
        for (int idx = burstStart; idx < burstEnd; ++idx) {
            float val = std::abs(line[static_cast<std::size_t>(idx)]);
            if (val > maxBurst) {
                maxBurst = val;
                maxBurstIdx = idx;
            }
        }
        const double scale = (burstRms > 0.0) ? config_.burstAbsRef / burstRms : 1.0;

        for (float& sample : line) {
            sample = static_cast<float>(sample * scale);
        }

        if (lineIdx < startLine + 3) {
                std::cout << "[ChromaProcessor] Samples:";
                for (int idx = burstStart; idx < std::min(burstEnd, burstStart + 5); ++idx) {
                    std::cout << ' ' << line[static_cast<std::size_t>(idx)] / static_cast<float>(scale);
                }
                std::cout << '\n';
            std::cout << "[ChromaProcessor] ACC line " << lineIdx
                      << ": RMS=" << burstRms
                      << " scale=" << scale
                      << " sumsq=" << sumSquares
                      << " max=" << maxBurst
                      << " idx=" << maxBurstIdx << '\n';
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
