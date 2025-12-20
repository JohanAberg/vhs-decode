#include "vhsdecode/tbc_writer.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iomanip>

namespace vhsdecode {
namespace io {

TBCWriter::TBCWriter(const std::string& basePath, bool writeChroma, bool writeMetadata)
    : basePath_(basePath)
    , writeChroma_(writeChroma)
    , writeMetadata_(writeMetadata)
    , fieldsWritten_(0)
{
    // Open video file
    std::string videoPath = basePath + ".tbc";
    videoFile_.open(videoPath, std::ios::binary);
    if (!videoFile_) {
        throw std::runtime_error("Failed to open video file: " + videoPath);
    }
    
    // Open chroma file if requested
    if (writeChroma_) {
        std::string chromaPath = basePath + "_chroma.tbc";
        chromaFile_.open(chromaPath, std::ios::binary);
        if (!chromaFile_) {
            videoFile_.close();
            throw std::runtime_error("Failed to open chroma file: " + chromaPath);
        }
    }
}

TBCWriter::~TBCWriter() {
    close();
}

TBCWriter::TBCWriter(TBCWriter&&) noexcept = default;
TBCWriter& TBCWriter::operator=(TBCWriter&&) noexcept = default;

bool TBCWriter::isOpen() const {
    return videoFile_.is_open();
}

void TBCWriter::writeVideoField(const VideoField& fieldData, size_t fieldNumber) {
    if (!videoFile_.is_open()) {
        throw std::runtime_error("Video file not open");
    }
    
    // Write each line of the field
    for (const auto& line : fieldData) {
        writeVideoSamples(line.data(), line.size());
    }
    
    fieldsWritten_++;
}

void TBCWriter::writeChromaField(const VideoField& chromaData, size_t fieldNumber) {
    if (!writeChroma_ || !chromaFile_.is_open()) {
        return;  // Silently skip if chroma not enabled
    }
    
    // Write each line of chroma data
    for (const auto& line : chromaData) {
        chromaFile_.write(reinterpret_cast<const char*>(line.data()), 
                         line.size() * sizeof(uint16_t));
    }
}

void TBCWriter::writeVideoSamples(const uint16_t* data, size_t count) {
    if (!videoFile_.is_open()) {
        throw std::runtime_error("Video file not open");
    }
    
    videoFile_.write(reinterpret_cast<const char*>(data), count * sizeof(uint16_t));
}

void TBCWriter::addFieldMetadata(size_t fieldNum, const FieldMetadata& metadata) {
    if (writeMetadata_) {
        MetadataEntry entry;
        entry.fieldNumber = fieldNum;
        entry.metadata = metadata;
        metadataEntries_.push_back(entry);
    }
}

void TBCWriter::close() {
    if (videoFile_.is_open()) {
        videoFile_.close();
    }
    
    if (chromaFile_.is_open()) {
        chromaFile_.close();
    }
    
    // Write metadata file if requested
    if (writeMetadata_ && !metadataEntries_.empty()) {
        writeMetadataFile();
    }
}

void TBCWriter::setVideoParameters(const VideoParameters& params) {
    videoParams_ = params;
}

void TBCWriter::setPcmAudioParameters(const PcmAudioParameters& params) {
    audioParams_ = params;
}

std::string TBCWriter::escapeJson(const std::string& str) const {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

void TBCWriter::writeMetadataFile() {
    std::string metadataPath = basePath_ + ".tbc.json";
    std::ofstream json(metadataPath);
    
    if (!json) {
        return; // Silently fail for metadata
    }
    
    json << std::fixed << std::setprecision(1);
    
    // Start JSON object
    json << "{";
    
    // PCM Audio Parameters
    json << "\"pcmAudioParameters\":{";
    json << "\"bits\":" << audioParams_.bits << ",";
    json << "\"isLittleEndian\":" << (audioParams_.isLittleEndian ? "true" : "false") << ",";
    json << "\"isSigned\":" << (audioParams_.isSigned ? "true" : "false") << ",";
    json << "\"sampleRate\":" << audioParams_.sampleRate;
    json << "},";
    
    // Video Parameters
    json << "\"videoParameters\":{";
    json << "\"numberOfSequentialFields\":" << videoParams_.numberOfSequentialFields << ",";
    json << "\"osInfo\":\"" << escapeJson(videoParams_.osInfo) << "\",";
    json << "\"gitBranch\":\"" << escapeJson(videoParams_.gitBranch) << "\",";
    json << "\"gitCommit\":\"" << escapeJson(videoParams_.gitCommit) << "\",";
    json << "\"system\":\"" << escapeJson(videoParams_.system) << "\",";
    json << "\"fieldWidth\":" << videoParams_.fieldWidth << ",";
    json << "\"sampleRate\":" << std::setprecision(1) << videoParams_.sampleRate << ",";
    json << "\"black16bIre\":" << std::setprecision(1) << videoParams_.black16bIre << ",";
    json << "\"white16bIre\":" << std::setprecision(6) << videoParams_.white16bIre << ",";
    json << "\"fieldHeight\":" << videoParams_.fieldHeight << ",";
    json << "\"colourBurstStart\":" << videoParams_.colourBurstStart << ",";
    json << "\"colourBurstEnd\":" << videoParams_.colourBurstEnd << ",";
    json << "\"activeVideoStart\":" << videoParams_.activeVideoStart << ",";
    json << "\"activeVideoEnd\":" << videoParams_.activeVideoEnd << ",";
    json << "\"tapeFormat\":\"" << escapeJson(videoParams_.tapeFormat) << "\"";
    json << "},";
    
    // Fields Array
    json << "\"fields\":[";
    for (size_t i = 0; i < metadataEntries_.size(); ++i) {
        const auto& entry = metadataEntries_[i];
        const auto& meta = entry.metadata;
        
        if (i > 0) json << ",";
        
        json << "{";
        json << "\"isFirstField\":" << (meta.isFirstField ? "true" : "false") << ",";
        json << "\"syncConf\":" << meta.syncConf << ",";
        json << "\"seqNo\":" << meta.seqNo << ",";
        json << "\"diskLoc\":" << std::setprecision(1) << meta.diskLoc << ",";
        json << "\"fileLoc\":" << meta.fileLoc << ",";
        json << "\"fieldPhaseID\":" << meta.fieldPhaseID << ",";
        json << "\"decodeFaults\":" << meta.decodeFaults;
        
        // Add VITS metrics if available
        if (meta.vitsMetricsBPSNR > 0.0) {
            json << ",\"vitsMetrics\":{";
            json << "\"bPSNR\":" << std::setprecision(1) << meta.vitsMetricsBPSNR;
            json << "}";
        }
        
        json << "}";
    }
    json << "]";
    
    // End JSON object
    json << "}";
    
    json.close();
}

} // namespace io
} // namespace vhsdecode
