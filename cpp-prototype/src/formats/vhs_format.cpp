#include "formats/vhs_format.hpp"
#include <stdexcept>

namespace vhsdecode {
namespace formats {

FormatParams VHSFormat::getFormatParams() const {
    FormatParams params;
    params.format = TapeFormat::VHS;
    params.headCount = 2;  // VHS has 2 video heads
    return params;
}

SystemParams VHSFormat::getSystemParams(TVSystem system) const {
    SystemParams params;
    params.system = system;
    
    switch (system) {
        case TVSystem::NTSC:
            // NTSC color subcarrier: 315/88 MHz ≈ 3.579545 MHz
            params.fscMHz = 315.0 / 88.0;
            params.frameLines = 525;
            params.fieldLines[0] = 263;
            params.fieldLines[1] = 262;
            // Line period for NTSC: 63.556 μs (227.5 color cycles)
            params.linePeriodUs = 1000000.0 / (params.fscMHz * 227.5);
            params.ire0 = 8100000;
            params.hzPerIRE = 1700000.0 / 140.0;
            params.colorBurstStartUs = 5.3;
            params.colorBurstEndUs = 7.6;
            params.burstAbsRef = 4416.0;
            params.enableComb = true;
            break;
            
        case TVSystem::PAL:
            // PAL color subcarrier: 4.43361875 MHz
            params.fscMHz = ((1.0 / 64.0) * 283.75) + (25.0 / 1000000.0);
            params.frameLines = 625;
            params.fieldLines[0] = 312;
            params.fieldLines[1] = 313;
            // Line period for PAL: 64 μs
            params.linePeriodUs = 64.0;
            params.ire0 = 7100000;
            params.hzPerIRE = 800000.0 / 100.0;
            params.colorBurstStartUs = 5.6;
            params.colorBurstEndUs = 7.85;
            params.burstAbsRef = 5000.0;
            params.enableComb = true;
            break;
            
        case TVSystem::PAL_M:
            // PAL-M uses NTSC line/field structure with PAL-like color
            params.fscMHz = 315.0 / 88.0 * 1.001;  // Approximation
            params.frameLines = 525;
            params.fieldLines[0] = 263;
            params.fieldLines[1] = 262;
            params.linePeriodUs = 63.556;
            params.ire0 = 8100000;
            params.hzPerIRE = 1700000.0 / 140.0;
            params.colorBurstStartUs = 5.3;
            params.colorBurstEndUs = 7.6;
            params.burstAbsRef = 4416.0;
            params.enableComb = true;
            break;
            
        default:
            throw std::invalid_argument("Unsupported TV system for VHS");
    }
    
    return params;
}

double VHSFormat::getVideoCarrierMHz(TVSystem system) const {
    switch (system) {
        case TVSystem::NTSC:
            return 3.4;  // 3.4 MHz video carrier for NTSC VHS
        case TVSystem::PAL:
            return 3.8;  // 3.8 MHz video carrier for PAL VHS
        case TVSystem::PAL_M:
            return 3.4;  // Same as NTSC
        default:
            throw std::invalid_argument("Unsupported TV system for VHS");
    }
}

double VHSFormat::getChromaCarrierMHz(TVSystem system) const {
    switch (system) {
        case TVSystem::NTSC:
            return 629.0 / 1000.0;  // 629 kHz for NTSC VHS
        case TVSystem::PAL:
            return 626.0 / 1000.0;  // 626 kHz for PAL VHS
        case TVSystem::PAL_M:
            return 629.0 / 1000.0;  // Same as NTSC
        default:
            throw std::invalid_argument("Unsupported TV system for VHS");
    }
}

ProcessingConfig VHSFormat::createConfig(TVSystem system, double inputFreqMHz) const {
    ProcessingConfig config;
    config.inputFreqMHz = inputFreqMHz;
    config.blockSize = 32768;  // 32K samples per block
    config.system = getSystemParams(system);
    config.format = getFormatParams();
    
    // Set format-specific carrier frequencies
    config.format.videoCarrierMHz = getVideoCarrierMHz(system);
    config.format.chromaCarrierMHz = getChromaCarrierMHz(system);
    
    // VHS video deviation is approximately 1 MHz peak
    config.format.videoDeviationMHz = 1.0;
    
    return config;
}

} // namespace formats
} // namespace vhsdecode
