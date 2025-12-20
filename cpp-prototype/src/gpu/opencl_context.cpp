#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>

namespace vhsdecode {
namespace gpu {

OpenCLContext::OpenCLContext() {
    initializeDefaultDevice();
}

OpenCLContext::OpenCLContext(size_t platformIdx, size_t deviceIdx) {
    initializeWithDevice(platformIdx, deviceIdx);
}

OpenCLContext::~OpenCLContext() {
    // Cleanup handled by cl:: destructors
}

void OpenCLContext::initializeDefaultDevice() {
    auto bestDevice = selectBestDevice();
    initializeWithDevice(bestDevice.platformIndex, bestDevice.deviceIndex);
}

void OpenCLContext::initializeWithDevice(size_t platformIdx, size_t deviceIdx) {
    try {
        // Get platforms
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        
        if (platforms.empty()) {
            throw std::runtime_error("No OpenCL platforms found");
        }
        
        if (platformIdx >= platforms.size()) {
            throw std::runtime_error("Invalid platform index");
        }
        
        platform_ = platforms[platformIdx];
        
        // Get devices
        std::vector<cl::Device> devices;
        platform_.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        
        if (devices.empty()) {
            throw std::runtime_error("No OpenCL devices found");
        }
        
        if (deviceIdx >= devices.size()) {
            throw std::runtime_error("Invalid device index");
        }
        
        device_ = devices[deviceIdx];
        
        // Create context
        context_ = cl::Context(device_);
        
        // Create command queue with profiling
        queue_ = cl::CommandQueue(context_, device_, CL_QUEUE_PROFILING_ENABLE);
        
        std::cout << "OpenCL initialized:" << std::endl;
        std::cout << "  Platform: " << getPlatformName() << std::endl;
        std::cout << "  Device: " << getDeviceName() << std::endl;
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error during initialization: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

std::string OpenCLContext::getDeviceName() const {
    try {
        return device_.getInfo<CL_DEVICE_NAME>();
    } catch (const cl::Error& e) {
        return "Unknown";
    }
}

std::string OpenCLContext::getPlatformName() const {
    try {
        return platform_.getInfo<CL_PLATFORM_NAME>();
    } catch (const cl::Error& e) {
        return "Unknown";
    }
}

cl_device_type OpenCLContext::getDeviceType() const {
    try {
        return device_.getInfo<CL_DEVICE_TYPE>();
    } catch (const cl::Error& e) {
        return CL_DEVICE_TYPE_DEFAULT;
    }
}

std::string OpenCLContext::getDeviceTypeString() const {
    return deviceTypeToString(getDeviceType());
}

size_t OpenCLContext::getMaxWorkGroupSize() const {
    try {
        return device_.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    } catch (const cl::Error& e) {
        return 0;
    }
}

cl_ulong OpenCLContext::getGlobalMemSize() const {
    try {
        return device_.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
    } catch (const cl::Error& e) {
        return 0;
    }
}

cl_ulong OpenCLContext::getLocalMemSize() const {
    try {
        return device_.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
    } catch (const cl::Error& e) {
        return 0;
    }
}

cl_uint OpenCLContext::getComputeUnits() const {
    try {
        return device_.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    } catch (const cl::Error& e) {
        return 0;
    }
}

cl_uint OpenCLContext::getMaxClockFrequency() const {
    try {
        return device_.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
    } catch (const cl::Error& e) {
        return 0;
    }
}

cl::Buffer OpenCLContext::createBuffer(size_t size, cl_mem_flags flags) {
    try {
        return cl::Buffer(context_, flags, size);
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "Failed to create buffer: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

void OpenCLContext::writeBuffer(cl::Buffer& buffer, const void* data, size_t size) {
    try {
        queue_.enqueueWriteBuffer(buffer, CL_TRUE, 0, size, data);
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "Failed to write buffer: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

void OpenCLContext::readBuffer(cl::Buffer& buffer, void* data, size_t size) {
    try {
        queue_.enqueueReadBuffer(buffer, CL_TRUE, 0, size, data);
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "Failed to read buffer: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

std::vector<PlatformInfo> OpenCLContext::enumeratePlatforms() {
    std::vector<PlatformInfo> result;
    
    try {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        
        for (size_t i = 0; i < platforms.size(); ++i) {
            PlatformInfo info;
            info.index = i;
            info.name = platforms[i].getInfo<CL_PLATFORM_NAME>();
            info.vendor = platforms[i].getInfo<CL_PLATFORM_VENDOR>();
            info.version = platforms[i].getInfo<CL_PLATFORM_VERSION>();
            
            std::vector<cl::Device> devices;
            platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &devices);
            for (size_t j = 0; j < devices.size(); ++j) {
                info.deviceIndices.push_back(j);
            }
            
            result.push_back(info);
        }
    } catch (const cl::Error& e) {
        std::cerr << "Error enumerating platforms: " << e.what() << std::endl;
    }
    
    return result;
}

std::vector<DeviceInfo> OpenCLContext::enumerateDevices(const cl::Platform& platform, size_t platformIdx) {
    std::vector<DeviceInfo> result;
    
    try {
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        
        for (size_t i = 0; i < devices.size(); ++i) {
            DeviceInfo info;
            info.platformIndex = platformIdx;
            info.deviceIndex = i;
            info.name = devices[i].getInfo<CL_DEVICE_NAME>();
            info.type = devices[i].getInfo<CL_DEVICE_TYPE>();
            info.typeString = deviceTypeToString(info.type);
            info.computeUnits = devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
            info.globalMemSize = devices[i].getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
            info.localMemSize = devices[i].getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
            info.maxWorkGroupSize = devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
            info.maxClockFrequency = devices[i].getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
            info.performanceScore = scoreDevice(devices[i]);
            
            result.push_back(info);
        }
    } catch (const cl::Error& e) {
        std::cerr << "Error enumerating devices: " << e.what() << std::endl;
    }
    
    return result;
}

DeviceInfo OpenCLContext::selectBestDevice() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    
    if (platforms.empty()) {
        throw std::runtime_error("No OpenCL platforms found");
    }
    
    DeviceInfo bestDevice;
    int bestScore = -1;
    
    for (size_t i = 0; i < platforms.size(); ++i) {
        auto devices = enumerateDevices(platforms[i], i);
        
        for (const auto& device : devices) {
            if (device.performanceScore > bestScore) {
                bestScore = device.performanceScore;
                bestDevice = device;
            }
        }
    }
    
    if (bestScore < 0) {
        throw std::runtime_error("No suitable OpenCL device found");
    }
    
    return bestDevice;
}

int OpenCLContext::scoreDevice(const cl::Device& device) {
    cl_device_type type = device.getInfo<CL_DEVICE_TYPE>();
    cl_uint computeUnits = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    
    int score = 0;
    
    if (type & CL_DEVICE_TYPE_GPU) {
        // GPUs get high priority
        score = 1000 + computeUnits * 10;
    } else if (type & CL_DEVICE_TYPE_ACCELERATOR) {
        // Accelerators (e.g., FPGAs) get medium-high priority
        score = 500 + computeUnits * 5;
    } else if (type & CL_DEVICE_TYPE_CPU) {
        // CPUs get low priority
        score = 100 + computeUnits;
    } else {
        // Default/Custom devices
        score = computeUnits;
    }
    
    return score;
}

std::string OpenCLContext::deviceTypeToString(cl_device_type type) {
    if (type & CL_DEVICE_TYPE_GPU) return "GPU";
    if (type & CL_DEVICE_TYPE_CPU) return "CPU";
    if (type & CL_DEVICE_TYPE_ACCELERATOR) return "ACCELERATOR";
    if (type & CL_DEVICE_TYPE_CUSTOM) return "CUSTOM";
    return "DEFAULT";
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
