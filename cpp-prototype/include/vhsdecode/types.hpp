#pragma once

#include <cstdint>
#include <vector>
#include <complex>
#include <memory>
#include <string>

namespace vhsdecode {

// Type aliases
using SampleU8 = uint8_t;
using SampleF32 = float;
using RealArray = std::vector<SampleF32>;
using ComplexArray = std::vector<std::complex<SampleF32>>;

// TV system types
enum class TVSystem {
    NTSC,
    PAL,
    PAL_M,
    SECAM
};

// Tape format types
enum class TapeFormat {
    VHS,
    SVHS,
    BETAMAX,
    VIDEO8,
    HI8,
    UMATIC
};

// System parameters
struct SystemParams {
    TVSystem system;
    double fscMHz;
    int frameLines;
    int fieldLines[2];
    double linePeriodUs;
    double ire0;
    double hzPerIRE;
};

// Format parameters
struct FormatParams {
    TapeFormat format;
    double videoCarrierMHz;
    double chromaCarrierMHz;
    double videoDeviationMHz;
    int headCount;
};

// RF block
struct RFBlock {
    std::vector<SampleU8> data;
    size_t blockNumber;
    size_t globalOffset;
    
    RFBlock(size_t size = 0, size_t num = 0, size_t offset = 0)
        : data(size), blockNumber(num), globalOffset(offset) {}
};

// Demodulated data
struct DemodBlock {
    RealArray video;
    RealArray chroma;
    RealArray envelope;
    size_t blockNumber;
};

// Processing config
struct ProcessingConfig {
    double inputFreqMHz;
    size_t blockSize;
    SystemParams system;
    FormatParams format;
    bool useGPU;
    int threadCount;
    
    ProcessingConfig()
        : inputFreqMHz(40.0)
        , blockSize(32768)
        , useGPU(false)
        , threadCount(4)
    {}
};

} // namespace vhsdecode
