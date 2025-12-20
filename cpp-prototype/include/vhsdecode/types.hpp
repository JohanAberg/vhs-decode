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

using VideoLine = std::vector<uint16_t>;
using VideoField = std::vector<VideoLine>;

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

// Field metadata (matches Python ld-decode .tbc.json format)
struct FieldMetadata {
    int fieldNumber;
    bool isFirstField;
    int syncConf;           // Sync confidence (0-100)
    int seqNo;              // Sequential field number
    double diskLoc;         // Disk location in revolutions
    size_t fileLoc;         // File location in bytes
    int fieldPhaseID;       // Field phase identifier
    int decodeFaults;       // Number of decode faults
    int lineCount;          // Number of lines in field
    int dropoutCount;       // Number of dropouts detected
    double vitsMetricsBPSNR; // VITS metrics: Black Peak SNR
    
    FieldMetadata() 
        : fieldNumber(0)
        , isFirstField(true)
        , syncConf(100)
        , seqNo(0)
        , diskLoc(0.0)
        , fileLoc(0)
        , fieldPhaseID(1)
        , decodeFaults(0)
        , lineCount(0)
        , dropoutCount(0)
        , vitsMetricsBPSNR(0.0)
    {}
};

// Video parameters for metadata
struct VideoParameters {
    int numberOfSequentialFields;
    std::string system;           // "NTSC", "PAL", etc.
    std::string tapeFormat;       // "VHS", "SVHS", etc.
    int fieldWidth;               // Samples per line
    int fieldHeight;              // Lines per field
    double sampleRate;            // Samples per second
    double black16bIre;           // 16-bit black level
    double white16bIre;           // 16-bit white level
    int colourBurstStart;         // Colour burst start sample
    int colourBurstEnd;           // Colour burst end sample
    int activeVideoStart;         // Active video start sample
    int activeVideoEnd;           // Active video end sample
    std::string osInfo;           // OS information
    std::string gitBranch;        // Git branch
    std::string gitCommit;        // Git commit hash
    
    VideoParameters()
        : numberOfSequentialFields(0)
        , system("PAL")
        , tapeFormat("VHS")
        , fieldWidth(1135)
        , fieldHeight(313)
        , sampleRate(40000000.0)
        , black16bIre(14745.6)
        , white16bIre(59417.6)
        , colourBurstStart(98)
        , colourBurstEnd(138)
        , activeVideoStart(185)
        , activeVideoEnd(1107)
        , osInfo("Windows:11")
        , gitBranch("cpp-prototype")
        , gitCommit("unknown")
    {}
};

// PCM audio parameters for metadata
struct PcmAudioParameters {
    int bits;
    bool isLittleEndian;
    bool isSigned;
    int sampleRate;
    
    PcmAudioParameters()
        : bits(16)
        , isLittleEndian(true)
        , isSigned(true)
        , sampleRate(0)
    {}
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
