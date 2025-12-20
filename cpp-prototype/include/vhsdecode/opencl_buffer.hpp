#pragma once

#ifdef HAVE_OPENCL

#include "opencl_context.hpp"

namespace vhsdecode {
namespace gpu {

/**
 * @brief RAII wrapper for OpenCL buffer
 * 
 * Manages GPU memory allocation and data transfer
 */
class OpenCLBuffer {
public:
    /**
     * @brief Create GPU buffer
     * @param ctx OpenCL context
     * @param size Buffer size in bytes
     * @param flags Memory flags (CL_MEM_READ_WRITE, etc.)
     */
    OpenCLBuffer(OpenCLContext& ctx, size_t size, cl_mem_flags flags);
    
    /**
     * @brief Destructor - buffer cleanup handled by cl::Buffer
     */
    ~OpenCLBuffer() = default;
    
    // Delete copy
    OpenCLBuffer(const OpenCLBuffer&) = delete;
    OpenCLBuffer& operator=(const OpenCLBuffer&) = delete;
    
    // Allow move
    OpenCLBuffer(OpenCLBuffer&&) noexcept = default;
    OpenCLBuffer& operator=(OpenCLBuffer&&) noexcept = default;
    
    /**
     * @brief Write data from host to device
     * @param data Host data pointer
     * @param size Data size in bytes (must be <= buffer size)
     */
    void writeFromHost(const void* data, size_t size);
    
    /**
     * @brief Read data from device to host
     * @param data Host data pointer
     * @param size Data size in bytes (must be <= buffer size)
     */
    void readToHost(void* data, size_t size);
    
    /**
     * @brief Get buffer size in bytes
     */
    size_t getSize() const { return size_; }
    
    /**
     * @brief Get underlying cl::Buffer
     */
    cl::Buffer& getBuffer() { return buffer_; }
    const cl::Buffer& getBuffer() const { return buffer_; }
    
private:
    OpenCLContext& context_;
    cl::Buffer buffer_;
    size_t size_;
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
