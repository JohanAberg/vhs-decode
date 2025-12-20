#ifdef HAVE_OPENCL

#include "vhsdecode/gpu/envelope_kernel.hpp"
#include "vhsdecode/gpu/kernel_loader.hpp"
#include <stdexcept>
#include <sstream>
#include <cmath>

namespace vhsdecode {
namespace gpu {

// Embedded kernel source
static const char* envelopeKernelSource = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__kernel void envelope_detect(
    __global const double2* analytic_signal,
    __global double* envelope,
    const int N
) {
    int idx = get_global_id(0);
    if (idx < N) {
        double2 z = analytic_signal[idx];
        envelope[idx] = sqrt(z.x * z.x + z.y * z.y);
    }
}

__kernel void envelope_detect_fast(
    __global const double2* analytic_signal,
    __global double* envelope,
    const int N
) {
    int idx = get_global_id(0);
    if (idx < N) {
        double2 z = analytic_signal[idx];
        envelope[idx] = length(z);
    }
}

__kernel void envelope_detect_filtered(
    __global const double2* analytic_signal,
    __global double* envelope,
    const int N,
    const int window_size
) {
    int idx = get_global_id(0);
    if (idx < N) {
        int half_window = window_size / 2;
        double sum = 0.0;
        int count = 0;
        
        for (int i = -half_window; i <= half_window; ++i) {
            int sample_idx = idx + i;
            if (sample_idx >= 0 && sample_idx < N) {
                double2 sample = analytic_signal[sample_idx];
                sum += sqrt(sample.x * sample.x + sample.y * sample.y);
                count++;
            }
        }
        envelope[idx] = sum / count;
    }
}

__kernel void instantaneous_frequency(
    __global const double2* analytic_signal,
    __global double* frequency,
    const int N,
    const double sample_rate
) {
    int idx = get_global_id(0);
    if (idx > 0 && idx < N) {
        double2 z1 = analytic_signal[idx - 1];
        double2 z2 = analytic_signal[idx];
        
        double phase1 = atan2(z1.y, z1.x);
        double phase2 = atan2(z2.y, z2.x);
        double delta = phase2 - phase1;
        
        if (delta > M_PI) delta -= 2.0 * M_PI;
        if (delta < -M_PI) delta += 2.0 * M_PI;
        
        frequency[idx] = delta * sample_rate / (2.0 * M_PI);
    } else if (idx == 0) {
        frequency[0] = 0.0;
    }
}
)";

EnvelopeKernel::EnvelopeKernel(OpenCLContext& ctx) 
    : ctx_(ctx), bufferSize_(0) {
    loadKernels();
}

void EnvelopeKernel::loadKernels() {
    program_ = KernelLoader::loadFromSource(ctx_, envelopeKernelSource);
    
    try {
        detectKernel_ = cl::Kernel(program_, "envelope_detect");
        detectFastKernel_ = cl::Kernel(program_, "envelope_detect_fast");
        detectFilteredKernel_ = cl::Kernel(program_, "envelope_detect_filtered");
        instantaneousFreqKernel_ = cl::Kernel(program_, "instantaneous_frequency");
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("Failed to create envelope kernels: ") + e.what());
    }
}

void EnvelopeKernel::allocateBuffers(size_t size) {
    if (size <= bufferSize_) return;
    
    tempAnalyticBuffer_ = ctx_.createBuffer(size * sizeof(cl_double2), CL_MEM_READ_ONLY);
    tempEnvelopeBuffer_ = ctx_.createBuffer(size * sizeof(double), CL_MEM_WRITE_ONLY);
    
    bufferSize_ = size;
}

std::vector<double> EnvelopeKernel::detect(const std::vector<std::complex<double>>& analyticSignal) {
    size_t N = analyticSignal.size();
    allocateBuffers(N);
    
    ctx_.writeBuffer(tempAnalyticBuffer_, analyticSignal.data(), N * sizeof(std::complex<double>));
    
    detectGPU(tempAnalyticBuffer_, tempEnvelopeBuffer_, N);
    
    std::vector<double> result(N);
    ctx_.readBuffer(tempEnvelopeBuffer_, result.data(), N * sizeof(double));
    
    return result;
}

void EnvelopeKernel::detectGPU(cl::Buffer& analyticBuffer, cl::Buffer& envelopeBuffer, size_t N) {
    try {
        detectKernel_.setArg(0, analyticBuffer);
        detectKernel_.setArg(1, envelopeBuffer);
        detectKernel_.setArg(2, static_cast<int>(N));
        
        cl::CommandQueue& queue = ctx_.getQueue();
        queue.enqueueNDRangeKernel(
            detectKernel_, 
            cl::NullRange, 
            cl::NDRange(N), 
            cl::NullRange
        );
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("OpenCL error in detectGPU: ") + e.what());
    }
}

void EnvelopeKernel::detectFilteredGPU(
    cl::Buffer& analyticBuffer,
    cl::Buffer& envelopeBuffer,
    size_t N,
    int windowSize
) {
    try {
        detectFilteredKernel_.setArg(0, analyticBuffer);
        detectFilteredKernel_.setArg(1, envelopeBuffer);
        detectFilteredKernel_.setArg(2, static_cast<int>(N));
        detectFilteredKernel_.setArg(3, windowSize);
        
        cl::CommandQueue& queue = ctx_.getQueue();
        queue.enqueueNDRangeKernel(
            detectFilteredKernel_, 
            cl::NullRange, 
            cl::NDRange(N), 
            cl::NullRange
        );
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("OpenCL error in detectFilteredGPU: ") + e.what());
    }
}

void EnvelopeKernel::computeInstantaneousFrequency(
    cl::Buffer& analyticBuffer,
    cl::Buffer& frequencyBuffer,
    size_t N,
    double sampleRate
) {
    try {
        instantaneousFreqKernel_.setArg(0, analyticBuffer);
        instantaneousFreqKernel_.setArg(1, frequencyBuffer);
        instantaneousFreqKernel_.setArg(2, static_cast<int>(N));
        instantaneousFreqKernel_.setArg(3, sampleRate);
        
        cl::CommandQueue& queue = ctx_.getQueue();
        queue.enqueueNDRangeKernel(
            instantaneousFreqKernel_, 
            cl::NullRange, 
            cl::NDRange(N), 
            cl::NullRange
        );
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("OpenCL error in computeInstantaneousFrequency: ") + e.what());
    }
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
