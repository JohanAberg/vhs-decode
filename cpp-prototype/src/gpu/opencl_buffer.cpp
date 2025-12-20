#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_buffer.hpp"
#include <stdexcept>
#include <sstream>

namespace vhsdecode {
namespace gpu {

OpenCLBuffer::OpenCLBuffer(OpenCLContext& ctx, size_t size, cl_mem_flags flags)
    : context_(ctx)
    , buffer_(ctx.createBuffer(size, flags))
    , size_(size) {
}

void OpenCLBuffer::writeFromHost(const void* data, size_t size) {
    if (size > size_) {
        std::stringstream ss;
        ss << "Write size (" << size << ") exceeds buffer size (" << size_ << ")";
        throw std::invalid_argument(ss.str());
    }
    
    context_.writeBuffer(buffer_, data, size);
}

void OpenCLBuffer::readToHost(void* data, size_t size) {
    if (size > size_) {
        std::stringstream ss;
        ss << "Read size (" << size << ") exceeds buffer size (" << size_ << ")";
        throw std::invalid_argument(ss.str());
    }
    
    context_.readBuffer(buffer_, data, size);
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
