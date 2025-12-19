#pragma once

#include "vhsdecode/types.hpp"
#include <memory>
#include <string>

namespace vhsdecode {
namespace formats {

/**
 * @brief Abstract base class for tape format implementations
 */
class FormatBase {
public:
    virtual ~FormatBase() = default;
    
    virtual TapeFormat getFormat() const = 0;
    virtual std::string getFormatName() const = 0;
    virtual FormatParams getFormatParams() const = 0;
    virtual SystemParams getSystemParams(TVSystem system) const = 0;
    virtual bool isColorUnder() const = 0;
    virtual double getVideoCarrierMHz(TVSystem system) const = 0;
    virtual double getChromaCarrierMHz(TVSystem system) const = 0;
    virtual ProcessingConfig createConfig(TVSystem system, double inputFreqMHz = 40.0) const = 0;
};

// Factory function
std::unique_ptr<FormatBase> createFormat(TapeFormat format);

// Utility functions
TapeFormat formatFromString(const std::string& name);
TVSystem systemFromString(const std::string& name);
std::string formatToString(TapeFormat format);
std::string systemToString(TVSystem system);

} // namespace formats
} // namespace vhsdecode
