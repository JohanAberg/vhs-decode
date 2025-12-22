#pragma once

#include <cstddef>
#include <string>

#include "vhsdecode/types.hpp"

namespace vhsdecode {
namespace core {

struct DecoderPipelineOptions {
    TapeFormat format = TapeFormat::VHS;
    TVSystem system = TVSystem::NTSC;
    int threadCount = 4;
    int lengthFrames = 0;
    size_t seekOffset = 0;
    bool useGPU = false;
    bool usePrototypeFFT = false;
    bool alignToFirstField = true;
    std::string inputPath;
    std::string outputBasename;
};

struct DecoderPipelineResult {
    size_t processedBlocks = 0;
    size_t totalBlocks = 0;
    size_t writtenFields = 0;
    size_t totalSamplesProcessed = 0;
};

class DecoderObserver {
public:
    virtual ~DecoderObserver() = default;
    virtual void onLogMessage(const std::string& /*message*/) {}
    virtual void onProgress(size_t /*processedBlocks*/, size_t /*totalBlocks*/) {}
    virtual void onFieldWritten(const FieldMetadata& /*metadata*/) {}
};

DecoderPipelineResult runDecoderPipeline(const DecoderPipelineOptions& options,
                                         DecoderObserver* observer = nullptr);

} // namespace core
} // namespace vhsdecode
