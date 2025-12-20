#include "vhsdecode/tbc_writer.hpp"
#include <stdexcept>
#include <sstream>

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

void TBCWriter::writeMetadataFile() {
    std::string metadataPath = basePath_ + ".tbc.json";
    std::ofstream metadataFile(metadataPath);
    
    if (!metadataFile) {
        // Don't throw, just warn
        return;
    }
    
    // Write simple JSON metadata
    // For now, just write basic structure
    // TODO: Use nlohmann/json library for proper JSON generation
    metadataFile << "{\n";
    metadataFile << "  \"fields\": [\n";
    
    for (size_t i = 0; i < metadataEntries_.size(); ++i) {
        const auto& entry = metadataEntries_[i];
        metadataFile << "    {\n";
        metadataFile << "      \"fieldNumber\": " << entry.fieldNumber << ",\n";
        metadataFile << "      \"isFirstField\": " << (entry.metadata.isFirstField ? "true" : "false") << ",\n";
        metadataFile << "      \"lineCount\": " << entry.metadata.lineCount << ",\n";
        metadataFile << "      \"dropouts\": " << entry.metadata.dropoutCount << "\n";
        metadataFile << "    }";
        if (i < metadataEntries_.size() - 1) {
            metadataFile << ",";
        }
        metadataFile << "\n";
    }
    
    metadataFile << "  ],\n";
    metadataFile << "  \"totalFields\": " << fieldsWritten_ << "\n";
    metadataFile << "}\n";
    
    metadataFile.close();
}

} // namespace io
} // namespace vhsdecode
