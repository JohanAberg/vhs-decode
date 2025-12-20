#pragma once

#include "vhsdecode/types.hpp"
#include <string>
#include <memory>
#include <fstream>

namespace vhsdecode {
namespace io {

/**
 * @brief TBC (Time Base Corrected) file writer
 * 
 * Writes decoded video fields to TBC format files, which are 16-bit packed
 * video with optional chroma output and JSON metadata.
 */
class TBCWriter {
public:
    /**
     * @brief Create TBC writer for output
     * @param basePath Base path for output files (without extension)
     * @param writeChroma Whether to write separate chroma file
     * @param writeMetadata Whether to write JSON metadata
     */
    explicit TBCWriter(const std::string& basePath, 
                       bool writeChroma = true,
                       bool writeMetadata = true);
    
    /**
     * @brief Destructor - closes files
     */
    ~TBCWriter();
    
    // Disable copy, allow move
    TBCWriter(const TBCWriter&) = delete;
    TBCWriter& operator=(const TBCWriter&) = delete;
    TBCWriter(TBCWriter&&) noexcept;
    TBCWriter& operator=(TBCWriter&&) noexcept;
    
    /**
     * @brief Check if files are open and ready to write
     */
    bool isOpen() const;
    
    /**
     * @brief Write a video field (16-bit samples)
     * @param fieldData Field video data
     * @param fieldNumber Field number for metadata
     */
    void writeVideoField(const VideoField& fieldData, size_t fieldNumber);
    
    /**
     * @brief Write chroma data for a field
     * @param chromaData Chroma data
     * @param fieldNumber Field number
     */
    void writeChromaField(const VideoField& chromaData, size_t fieldNumber);
    
    /**
     * @brief Write raw 16-bit video samples
     * @param data Pointer to 16-bit samples
     * @param count Number of samples
     */
    void writeVideoSamples(const uint16_t* data, size_t count);
    
    /**
     * @brief Add field metadata
     * @param fieldNum Field number
     * @param metadata Field metadata
     */
    void addFieldMetadata(size_t fieldNum, const FieldMetadata& metadata);
    
    /**
     * @brief Get number of fields written
     */
    size_t getFieldsWritten() const { return fieldsWritten_; }
    
    /**
     * @brief Get base path
     */
    const std::string& getBasePath() const { return basePath_; }
    
    /**
     * @brief Close all files and finalize metadata
     */
    void close();

private:
    std::string basePath_;
    bool writeChroma_;
    bool writeMetadata_;
    
    std::ofstream videoFile_;
    std::ofstream chromaFile_;
    
    size_t fieldsWritten_;
    
    // Metadata storage (simple for now)
    struct MetadataEntry {
        size_t fieldNumber;
        FieldMetadata metadata;
    };
    std::vector<MetadataEntry> metadataEntries_;
    
    void writeMetadataFile();
};

} // namespace io
} // namespace vhsdecode
