#pragma once

#include "vhsdecode/types.hpp"
#include <vector>
#include <cstddef>

namespace vhsdecode {
namespace video {

/**
 * @brief Time-base correction and field assembly
 * 
 * Detects horizontal sync, normalizes line lengths, and assembles fields.
 */
class TBCCorrector {
public:
    struct TBCResult {
        std::vector<std::vector<double>> fields;  // Array of fields (each field = array of lines)
        std::vector<size_t> linesPerField;        // Lines in each field
        size_t totalLines;
    };

    /**
     * @brief Construct TBC corrector
     * @param system TV system (NTSC or PAL)
     * @param sampleRate Sample rate in MHz
     */
    TBCCorrector(TVSystem system, double sampleRate);

    /**
     * @brief Correct and assemble video into fields
     * @param video Input video signal
     * @return Assembled fields with corrected line lengths
     */
    TBCResult correctAndAssemble(const std::vector<double>& video);

    size_t getStandardLineLength() const { return standardLineLength_; }
    size_t getLinesPerField() const { return linesPerField_; }

private:
    TVSystem system_;
    double sampleRate_;
    size_t standardLineLength_;
    size_t linesPerField_;
    double hsyncThreshold_;

    std::vector<size_t> detectHsyncs(const std::vector<double>& video);
    std::vector<double> normalizeLine(const std::vector<double>& line, size_t targetLength);
};

} // namespace video
} // namespace vhsdecode
