#pragma once

#ifdef HAVE_OPENCL

#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include <CL/cl2.hpp>

#include <string>
#include <vector>
#include <memory>

namespace vhsdecode {
namespace gpu {

/**
 * @brief Information about an OpenCL platform
 */
struct PlatformInfo {
    size_t index;
    std::string name;
    std::string vendor;
    std::string version;
    std::vector<size_t> deviceIndices;
};

/**
 * @brief Information about an OpenCL device
 */
struct DeviceInfo {
    size_t platformIndex;
    size_t deviceIndex;
    std::string name;
    cl_device_type type;
    std::string typeString;
    cl_uint computeUnits;
    cl_ulong globalMemSize;
    cl_ulong localMemSize;
    size_t maxWorkGroupSize;
    cl_uint maxClockFrequency;
    int performanceScore;
};

/**
 * @brief Manages OpenCL context, device, and command queue
 * 
 * Provides automatic device selection, context creation, and resource management.
 * Uses RAII pattern for cleanup.
 */
class OpenCLContext {
public:
    /**
     * @brief Create context with automatically selected best device
     */
    OpenCLContext();
    
    /**
     * @brief Create context with manually selected device
     * @param platformIdx Platform index
     * @param deviceIdx Device index within platform
     */
    OpenCLContext(size_t platformIdx, size_t deviceIdx);
    
    /**
     * @brief Destructor - cleanup OpenCL resources
     */
    ~OpenCLContext();
    
    // Delete copy constructor and assignment
    OpenCLContext(const OpenCLContext&) = delete;
    OpenCLContext& operator=(const OpenCLContext&) = delete;
    
    // Allow move
    OpenCLContext(OpenCLContext&&) noexcept = default;
    OpenCLContext& operator=(OpenCLContext&&) noexcept = default;
    
    // Device information
    std::string getDeviceName() const;
    std::string getPlatformName() const;
    cl_device_type getDeviceType() const;
    std::string getDeviceTypeString() const;
    size_t getMaxWorkGroupSize() const;
    cl_ulong getGlobalMemSize() const;
    cl_ulong getLocalMemSize() const;
    cl_uint getComputeUnits() const;
    cl_uint getMaxClockFrequency() const;
    
    // Resource access
    cl::Context& getContext() { return context_; }
    cl::CommandQueue& getQueue() { return queue_; }
    cl::Device& getDevice() { return device_; }
    
    // Buffer creation
    cl::Buffer createBuffer(size_t size, cl_mem_flags flags);
    
    // Data transfer
    void writeBuffer(cl::Buffer& buffer, const void* data, size_t size);
    void readBuffer(cl::Buffer& buffer, void* data, size_t size);
    
    // Static utilities
    static std::vector<PlatformInfo> enumeratePlatforms();
    static std::vector<DeviceInfo> enumerateDevices(const cl::Platform& platform, size_t platformIdx);
    static DeviceInfo selectBestDevice();
    
private:
    cl::Platform platform_;
    cl::Device device_;
    cl::Context context_;
    cl::CommandQueue queue_;
    
    void initializeDefaultDevice();
    void initializeWithDevice(size_t platformIdx, size_t deviceIdx);
    static int scoreDevice(const cl::Device& device);
    static std::string deviceTypeToString(cl_device_type type);
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
