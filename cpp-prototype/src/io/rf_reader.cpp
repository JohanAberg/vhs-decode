#define NOMINMAX
#include "vhsdecode/rf_reader.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>

// Platform-specific includes for memory mapping
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace vhsdecode {
namespace io {

// Implementation class (pimpl pattern)
class RFReader::Impl {
public:
    #ifdef _WIN32
        HANDLE fileHandle = INVALID_HANDLE_VALUE;
        HANDLE mapHandle = NULL;
    #else
        int fileDescriptor = -1;
    #endif
    
    void* mappedData = nullptr;
    size_t mappedSize = 0;
    std::ifstream fileStream;
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        #ifdef _WIN32
            if (mappedData) {
                UnmapViewOfFile(mappedData);
                mappedData = nullptr;
            }
            if (mapHandle) {
                CloseHandle(mapHandle);
                mapHandle = NULL;
            }
            if (fileHandle != INVALID_HANDLE_VALUE) {
                CloseHandle(fileHandle);
                fileHandle = INVALID_HANDLE_VALUE;
            }
        #else
            if (mappedData && mappedData != MAP_FAILED) {
                munmap(mappedData, mappedSize);
                mappedData = nullptr;
            }
            if (fileDescriptor >= 0) {
                close(fileDescriptor);
                fileDescriptor = -1;
            }
        #endif
        
        if (fileStream.is_open()) {
            fileStream.close();
        }
    }
};

RFReader::RFReader(const std::string& filename, bool useMemoryMap)
    : impl_(std::make_unique<Impl>())
    , filename_(filename)
    , useMemoryMap_(useMemoryMap)
    , fileSize_(0)
    , position_(0)
{
    if (useMemoryMap_) {
        // Try memory-mapped I/O first
        #ifdef _WIN32
            impl_->fileHandle = CreateFileA(
                filename.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            
            if (impl_->fileHandle == INVALID_HANDLE_VALUE) {
                throw std::runtime_error("Failed to open file: " + filename);
            }
            
            LARGE_INTEGER size;
            if (!GetFileSizeEx(impl_->fileHandle, &size)) {
                impl_->cleanup();
                throw std::runtime_error("Failed to get file size: " + filename);
            }
            fileSize_ = static_cast<size_t>(size.QuadPart);
            
            impl_->mapHandle = CreateFileMappingA(
                impl_->fileHandle,
                NULL,
                PAGE_READONLY,
                0,
                0,
                NULL
            );
            
            if (!impl_->mapHandle) {
                impl_->cleanup();
                throw std::runtime_error("Failed to create file mapping: " + filename);
            }
            
            impl_->mappedData = MapViewOfFile(
                impl_->mapHandle,
                FILE_MAP_READ,
                0,
                0,
                0
            );
            
            if (!impl_->mappedData) {
                impl_->cleanup();
                throw std::runtime_error("Failed to map file: " + filename);
            }
            
            impl_->mappedSize = fileSize_;
        #else
            impl_->fileDescriptor = open(filename.c_str(), O_RDONLY);
            if (impl_->fileDescriptor < 0) {
                throw std::runtime_error("Failed to open file: " + filename);
            }
            
            struct stat st;
            if (fstat(impl_->fileDescriptor, &st) < 0) {
                impl_->cleanup();
                throw std::runtime_error("Failed to get file size: " + filename);
            }
            fileSize_ = st.st_size;
            impl_->mappedSize = fileSize_;
            
            impl_->mappedData = mmap(NULL, fileSize_, PROT_READ, MAP_PRIVATE, impl_->fileDescriptor, 0);
            if (impl_->mappedData == MAP_FAILED) {
                impl_->cleanup();
                throw std::runtime_error("Failed to map file: " + filename);
            }
            
            // Advise kernel about access pattern
            madvise(impl_->mappedData, fileSize_, MADV_SEQUENTIAL);
        #endif
    } else {
        // Fall back to standard file I/O
        impl_->fileStream.open(filename, std::ios::binary);
        if (!impl_->fileStream) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        
        impl_->fileStream.seekg(0, std::ios::end);
        fileSize_ = impl_->fileStream.tellg();
        impl_->fileStream.seekg(0, std::ios::beg);
    }
}

RFReader::~RFReader() = default;

RFReader::RFReader(RFReader&&) noexcept = default;
RFReader& RFReader::operator=(RFReader&&) noexcept = default;

bool RFReader::isOpen() const {
    if (useMemoryMap_) {
        return impl_->mappedData != nullptr;
    } else {
        return impl_->fileStream.is_open();
    }
}

size_t RFReader::getFileSize() const {
    return fileSize_;
}

size_t RFReader::getSampleCount(size_t sampleSize) const {
    return fileSize_ / sampleSize;
}

size_t RFReader::getPosition() const {
    return position_;
}

void RFReader::seek(size_t position) {
    if (position > fileSize_) {
        throw std::out_of_range("Seek position beyond file size");
    }
    
    position_ = position;
    
    if (!useMemoryMap_ && impl_->fileStream.is_open()) {
        impl_->fileStream.seekg(position);
    }
}

RFBlock RFReader::readBlock(size_t blockSize, size_t blockNumber) {
    RFBlock block(blockSize, blockNumber, position_);
    
    size_t bytesToRead = std::min(blockSize, fileSize_ - position_);
    if (bytesToRead == 0) {
        return block;  // Return empty block at EOF
    }
    
    block.data.resize(bytesToRead);
    
    if (useMemoryMap_ && impl_->mappedData) {
        // Memory-mapped: direct copy
        const uint8_t* src = static_cast<const uint8_t*>(impl_->mappedData) + position_;
        std::memcpy(block.data.data(), src, bytesToRead);
    } else if (impl_->fileStream.is_open()) {
        // Standard I/O: read from stream
        impl_->fileStream.read(reinterpret_cast<char*>(block.data.data()), bytesToRead);
        bytesToRead = impl_->fileStream.gcount();
        block.data.resize(bytesToRead);
    } else {
        throw std::runtime_error("File not open");
    }
    
    position_ += bytesToRead;
    return block;
}

size_t RFReader::readBytes(void* buffer, size_t count) {
    if (!buffer) {
        throw std::invalid_argument("Null buffer");
    }
    
    size_t bytesToRead = std::min(count, fileSize_ - position_);
    if (bytesToRead == 0) {
        return 0;
    }
    
    if (useMemoryMap_ && impl_->mappedData) {
        const uint8_t* src = static_cast<const uint8_t*>(impl_->mappedData) + position_;
        std::memcpy(buffer, src, bytesToRead);
    } else if (impl_->fileStream.is_open()) {
        impl_->fileStream.read(static_cast<char*>(buffer), bytesToRead);
        bytesToRead = impl_->fileStream.gcount();
    } else {
        throw std::runtime_error("File not open");
    }
    
    position_ += bytesToRead;
    return bytesToRead;
}

bool RFReader::isEOF() const {
    return position_ >= fileSize_;
}

} // namespace io
} // namespace vhsdecode
