#include "formats/format_base.hpp"
#include "formats/vhs_format.hpp"
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace vhsdecode {
namespace formats {

std::unique_ptr<FormatBase> createFormat(TapeFormat format) {
    switch (format) {
        case TapeFormat::VHS:
            return std::make_unique<VHSFormat>();
        case TapeFormat::SVHS:
            // TODO: Implement SVHS
            throw std::runtime_error("SVHS format not yet implemented");
        case TapeFormat::BETAMAX:
            // TODO: Implement Betamax
            throw std::runtime_error("Betamax format not yet implemented");
        default:
            throw std::invalid_argument("Unknown tape format");
    }
}

TapeFormat formatFromString(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "vhs") return TapeFormat::VHS;
    if (lower == "svhs" || lower == "s-vhs") return TapeFormat::SVHS;
    if (lower == "betamax" || lower == "beta") return TapeFormat::BETAMAX;
    if (lower == "video8") return TapeFormat::VIDEO8;
    if (lower == "hi8" || lower == "hi-8") return TapeFormat::HI8;
    if (lower == "umatic" || lower == "u-matic") return TapeFormat::UMATIC;
    
    throw std::invalid_argument("Unknown format: " + name);
}

TVSystem systemFromString(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "ntsc") return TVSystem::NTSC;
    if (lower == "pal") return TVSystem::PAL;
    if (lower == "pal-m" || lower == "palm") return TVSystem::PAL_M;
    if (lower == "secam") return TVSystem::SECAM;
    
    throw std::invalid_argument("Unknown TV system: " + name);
}

std::string formatToString(TapeFormat format) {
    switch (format) {
        case TapeFormat::VHS: return "VHS";
        case TapeFormat::SVHS: return "SVHS";
        case TapeFormat::BETAMAX: return "Betamax";
        case TapeFormat::VIDEO8: return "Video8";
        case TapeFormat::HI8: return "Hi8";
        case TapeFormat::UMATIC: return "U-Matic";
        default: return "Unknown";
    }
}

std::string systemToString(TVSystem system) {
    switch (system) {
        case TVSystem::NTSC: return "NTSC";
        case TVSystem::PAL: return "PAL";
        case TVSystem::PAL_M: return "PAL-M";
        case TVSystem::SECAM: return "SECAM";
        default: return "Unknown";
    }
}

} // namespace formats
} // namespace vhsdecode
