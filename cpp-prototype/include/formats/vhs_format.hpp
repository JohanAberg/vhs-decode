#pragma once

#include "formats/format_base.hpp"

namespace vhsdecode {
namespace formats {

/**
 * @brief VHS format implementation
 */
class VHSFormat : public FormatBase {
public:
    VHSFormat() = default;
    ~VHSFormat() override = default;
    
    TapeFormat getFormat() const override { return TapeFormat::VHS; }
    std::string getFormatName() const override { return "VHS"; }
    
    FormatParams getFormatParams() const override;
    SystemParams getSystemParams(TVSystem system) const override;
    
    bool isColorUnder() const override { return true; }
    
    double getVideoCarrierMHz(TVSystem system) const override;
    double getChromaCarrierMHz(TVSystem system) const override;
    
    ProcessingConfig createConfig(TVSystem system, double inputFreqMHz = 40.0) const override;
};

} // namespace formats
} // namespace vhsdecode
