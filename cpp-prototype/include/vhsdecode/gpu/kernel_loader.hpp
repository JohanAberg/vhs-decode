#pragma once

#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include <string>
#include <fstream>
#include <sstream>

namespace vhsdecode {
namespace gpu {

/**
 * @brief Utility class for loading OpenCL kernel sources
 */
class KernelLoader {
public:
    /**
     * @brief Load kernel source from embedded string
     * @param source Kernel source code
     * @return Compiled OpenCL program
     */
    static cl::Program loadFromSource(OpenCLContext& ctx, const std::string& source);
    
    /**
     * @brief Load kernel source from file
     * @param filename Path to kernel file (.cl)
     * @return Compiled OpenCL program
     */
    static cl::Program loadFromFile(OpenCLContext& ctx, const std::string& filename);
    
    /**
     * @brief Get build log if compilation fails
     * @param program OpenCL program
     * @param device OpenCL device
     * @return Build log string
     */
    static std::string getBuildLog(const cl::Program& program, const cl::Device& device);

private:
    /**
     * @brief Read file contents
     * @param filename Path to file
     * @return File contents as string
     */
    static std::string readFile(const std::string& filename);
};

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
