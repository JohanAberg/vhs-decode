#ifdef HAVE_OPENCL

#include "vhsdecode/gpu/phase_unwrap_kernel.hpp"
#include "vhsdecode/gpu/kernel_loader.hpp"
#include <stdexcept>
#include <sstream>
#include <cmath>

namespace vhsdecode {
namespace gpu {

// Embedded kernel source
static const char* phaseUnwrapKernelSource = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__kernel void phase_unwrap_simple(
    __global const double* phase_in,
    __global double* phase_out,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx == 0) {
        phase_out[0] = phase_in[0];
    } else if (idx < N) {
        double delta = phase_in[idx] - phase_in[idx - 1];
        
        if (delta > M_PI) delta -= 2.0 * M_PI;
        if (delta < -M_PI) delta += 2.0 * M_PI;
        
        phase_out[idx] = phase_out[idx - 1] + delta;
    }
}

__kernel void phase_diff(
    __global const double* phase_in,
    __global double* phase_diff_out,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx > 0 && idx < N) {
        double delta = phase_in[idx] - phase_in[idx - 1];
        
        if (delta > M_PI) delta -= 2.0 * M_PI;
        if (delta < -M_PI) delta += 2.0 * M_PI;
        
        phase_diff_out[idx] = delta;
    } else if (idx == 0) {
        phase_diff_out[0] = phase_in[0];
    }
}

__kernel void phase_accumulate(
    __global const double* phase_diff,
    __global double* phase_out,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx == 0) {
        phase_out[0] = phase_diff[0];
    } else if (idx < N) {
        double sum = phase_diff[0];
        for (int i = 1; i <= idx; ++i) {
            sum += phase_diff[i];
        }
        phase_out[idx] = sum;
    }
}
)";

PhaseUnwrapKernel::PhaseUnwrapKernel(OpenCLContext& ctx) 
    : ctx_(ctx), bufferSize_(0) {
    loadKernels();
}

void PhaseUnwrapKernel::loadKernels() {
    program_ = KernelLoader::loadFromSource(ctx_, phaseUnwrapKernelSource);
    
    try {
        unwrapSimpleKernel_ = cl::Kernel(program_, "phase_unwrap_simple");
        phaseDiffKernel_ = cl::Kernel(program_, "phase_diff");
        phaseAccumulateKernel_ = cl::Kernel(program_, "phase_accumulate");
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("Failed to create phase unwrap kernels: ") + e.what());
    }
}

void PhaseUnwrapKernel::allocateBuffers(size_t size) {
    if (size <= bufferSize_) return;
    
    tempInBuffer_ = ctx_.createBuffer(size * sizeof(double), CL_MEM_READ_ONLY);
    tempOutBuffer_ = ctx_.createBuffer(size * sizeof(double), CL_MEM_WRITE_ONLY);
    tempDiffBuffer_ = ctx_.createBuffer(size * sizeof(double), CL_MEM_READ_WRITE);
    
    bufferSize_ = size;
}

std::vector<double> PhaseUnwrapKernel::unwrap(const std::vector<double>& phaseIn) {
    size_t N = phaseIn.size();
    allocateBuffers(N);
    
    ctx_.writeBuffer(tempInBuffer_, phaseIn.data(), N * sizeof(double));
    
    unwrapGPU(tempInBuffer_, tempOutBuffer_, N);
    
    std::vector<double> result(N);
    ctx_.readBuffer(tempOutBuffer_, result.data(), N * sizeof(double));
    
    return result;
}

void PhaseUnwrapKernel::unwrapGPU(cl::Buffer& phaseInBuffer, cl::Buffer& phaseOutBuffer, size_t N) {
    // For now, use the simple sequential kernel (single work item)
    // This is slow but correct. Parallel version requires prefix sum scan.
    try {
        unwrapSimpleKernel_.setArg(0, phaseInBuffer);
        unwrapSimpleKernel_.setArg(1, phaseOutBuffer);
        unwrapSimpleKernel_.setArg(2, static_cast<int>(N));
        
        cl::CommandQueue& queue = ctx_.getQueue();
        // Run as single work item for sequential dependency
        queue.enqueueNDRangeKernel(
            unwrapSimpleKernel_, 
            cl::NullRange, 
            cl::NDRange(1), 
            cl::NullRange
        );
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("OpenCL error in unwrapGPU: ") + e.what());
    }
}

void PhaseUnwrapKernel::computePhaseDiff(cl::Buffer& phaseIn, cl::Buffer& phaseDiff, size_t N) {
    try {
        phaseDiffKernel_.setArg(0, phaseIn);
        phaseDiffKernel_.setArg(1, phaseDiff);
        phaseDiffKernel_.setArg(2, static_cast<int>(N));
        
        cl::CommandQueue& queue = ctx_.getQueue();
        queue.enqueueNDRangeKernel(
            phaseDiffKernel_, 
            cl::NullRange, 
            cl::NDRange(N), 
            cl::NullRange
        );
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("OpenCL error in computePhaseDiff: ") + e.what());
    }
}

void PhaseUnwrapKernel::accumulatePhaseDiff(cl::Buffer& phaseDiff, cl::Buffer& phaseOut, size_t N) {
    // Sequential accumulation (single work item)
    try {
        phaseAccumulateKernel_.setArg(0, phaseDiff);
        phaseAccumulateKernel_.setArg(1, phaseOut);
        phaseAccumulateKernel_.setArg(2, static_cast<int>(N));
        
        cl::CommandQueue& queue = ctx_.getQueue();
        queue.enqueueNDRangeKernel(
            phaseAccumulateKernel_, 
            cl::NullRange, 
            cl::NDRange(1), 
            cl::NullRange
        );
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("OpenCL error in accumulatePhaseDiff: ") + e.what());
    }
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
