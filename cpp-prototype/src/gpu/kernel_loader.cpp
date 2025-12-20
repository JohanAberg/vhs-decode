#ifdef HAVE_OPENCL

#include "vhsdecode/gpu/kernel_loader.hpp"
#include <iostream>
#include <stdexcept>

namespace vhsdecode {
namespace gpu {

std::string KernelLoader::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open kernel file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

cl::Program KernelLoader::loadFromSource(OpenCLContext& ctx, const std::string& source) {
    try {
        cl::Program::Sources sources;
        sources.push_back({source.c_str(), source.length()});
        
        cl::Program program(ctx.getContext(), sources);
        program.build({ctx.getDevice()});
        
        return program;
        
    } catch (const cl::Error& e) {
        if (e.err() == CL_BUILD_PROGRAM_FAILURE) {
            std::string buildLog = getBuildLog(cl::Program(), ctx.getDevice());
            std::stringstream ss;
            ss << "OpenCL kernel build failed:\n" << buildLog;
            throw std::runtime_error(ss.str());
        }
        throw;
    }
}

cl::Program KernelLoader::loadFromFile(OpenCLContext& ctx, const std::string& filename) {
    std::string source = readFile(filename);
    return loadFromSource(ctx, source);
}

std::string KernelLoader::getBuildLog(const cl::Program& program, const cl::Device& device) {
    try {
        return program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
    } catch (...) {
        return "(build log unavailable)";
    }
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
