#pragma once

#include "vhsdecode/types.hpp"
#include <string>
#include <memory>
#include <cstddef>

namespace vhsdecode {
namespace io {

/**
 * @brief RF file reader with memory-mapped I/O for efficient large file access
 * 
 * Supports reading raw RF samples in block-based fashion with optional
 * memory mapping for zero-copy access to large capture files.
 */
class RFReader {
public:
    /**
     * @brief Open an RF capture file
     * @param filename Path to RF file
     * @param useMemoryMap Use memory-mapped I/O (default: true)
     */
    explicit RFReader(const std::string& filename, bool useMemoryMap = true);
    
    /**
     * @brief Destructor - closes file and unmaps memory
     */
    ~RFReader();
    
    // Disable copy, allow move
    RFReader(const RFReader&) = delete;
    RFReader& operator=(const RFReader&) = delete;
    RFReader(RFReader&&) noexcept;
    RFReader& operator=(RFReader&&) noexcept;
    
    /**
     * @brief Check if file is open and ready to read
     */
    bool isOpen() const;
    
    /**
     * @brief Get total file size in bytes
     */
    size_t getFileSize() const;
    
    /**
     * @brief Get total number of samples in file
     * @param sampleSize Size of one sample in bytes (1 for uint8, 2 for int16, etc.)
     */
    size_t getSampleCount(size_t sampleSize = 1) const;
    
    /**
     * @brief Get current read position in bytes
     */
    size_t getPosition() const;
    
    /**
     * @brief Seek to specific byte position
     * @param position Byte offset from start of file
     */
    void seek(size_t position);
    
    /**
     * @brief Read a block of raw RF data
     * @param blockSize Number of samples to read
     * @param blockNumber Block number for tracking
     * @return RFBlock containing the data
     */
    RFBlock readBlock(size_t blockSize, size_t blockNumber = 0);
    
    /**
     * @brief Read raw bytes into buffer
     * @param buffer Destination buffer
     * @param count Number of bytes to read
     * @return Number of bytes actually read
     */
    size_t readBytes(void* buffer, size_t count);
    
    /**
     * @brief Check if end of file reached
     */
    bool isEOF() const;
    
    /**
     * @brief Get filename
     */
    const std::string& getFilename() const { return filename_; }
    
    /**
     * @brief Check if using memory-mapped I/O
     */
    bool isMemoryMapped() const { return useMemoryMap_; }

private:
    class Impl;  // Forward declaration for pimpl
    std::unique_ptr<Impl> impl_;
    
    std::string filename_;
    bool useMemoryMap_;
    size_t fileSize_;
    size_t position_;
};

} // namespace io
} // namespace vhsdecode
