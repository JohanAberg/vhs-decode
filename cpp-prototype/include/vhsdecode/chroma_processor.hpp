#pragma once

#include "vhsdecode/types.hpp"
#include "vhsdecode/iir_filter.hpp"
#include <vector>
#include <memory>
#include <array>
#include <algorithm>

namespace vhsdecode {
namespace chroma {

class ChromaProcessor {
public:
    struct Config {
        double heterodyneFreq; // e.g. 5.06 MHz
        double sampleRate;     // e.g. 17.73 MHz
        size_t lineLength;     // e.g. 1135
        std::array<int, 2> chromaRotation{0, 0};
        int startingPhase = 0;
        bool enableComb = true;
        int combDelay = 2;
        int combStartLine = 16;
        int accStartLine = 16;
        int burstStartSample = 0;
        int burstEndSample = 0;
        double burstAbsRef = 0.0;
        double phaseOffset = 0.0; // Phase offset in radians
        
        // SOS coefficients for FChromaFinal (Bandpass 3-5.5MHz)
        std::vector<std::array<double, 6>> filterSOS;
    };

    ChromaProcessor(const Config& config);
    ~ChromaProcessor();

    /**
     * @brief Process a field of TBC'd chroma lines
     * 
     * Applies heterodyning (up-conversion) and bandpass filtering.
     * 
     * @param tbcChroma Input chroma lines (color-under frequency)
     * @param trackPhase Track phase index (0 or 1 for PAL VHS)
     * @return Processed chroma lines (subcarrier frequency)
     */
    std::vector<RealArray> processField(const std::vector<RealArray>& tbcChroma, int fieldNumber, int trackPhase);

private:
    Config config_;
    std::unique_ptr<filter::SOSFilter> bandpassFilter_;

    void applyHeterodyne(std::vector<RealArray>& field, int phaseRotation);
    void applyFilter(std::vector<RealArray>& field);
    void applyComb(std::vector<RealArray>& field) const;
    void applyAcc(std::vector<RealArray>& field) const;
    static double computeRMS(const RealArray& line, int startSample, int endSample);
};

} // namespace chroma
} // namespace vhsdecode
