#ifdef HAVE_OPENCL

#include "vhsdecode/gpu/filter_kernel.hpp"
#include "vhsdecode/gpu/kernel_loader.hpp"
#include <stdexcept>
#include <sstream>

namespace vhsdecode {
namespace gpu {

// Embedded kernel source
static const char* filterKernelSource = R"(
__kernel void apply_filter_freq_domain(
    __global double2* fft_data,
    __global const double* filter_response,
    const int N
) {
    int idx = get_global_id(0);
    if (idx < N) {
        double2 data = fft_data[idx];
        double gain = filter_response[idx];
        fft_data[idx].x = data.x * gain;
        fft_data[idx].y = data.y * gain;
    }
}

__kernel void apply_complex_filter_freq_domain(
    __global double2* fft_data,
    __global const double2* filter_response,
    const int N
) {
    int idx = get_global_id(0);
    if (idx < N) {
        double2 data = fft_data[idx];
        double2 filter = filter_response[idx];
        double real = data.x * filter.x - data.y * filter.y;
        double imag = data.x * filter.y + data.y * filter.x;
        fft_data[idx].x = real;
        fft_data[idx].y = imag;
    }
}

__kernel void apply_multiple_filters(
    __global double2* fft_data,
    __global const double* filters,
    const int num_filters,
    const int N
) {
    int idx = get_global_id(0);
    if (idx < N) {
        double2 data = fft_data[idx];
        for (int f = 0; f < num_filters; ++f) {
            double gain = filters[f * N + idx];
            data.x *= gain;
            data.y *= gain;
        }
        fft_data[idx] = data;
    }
}

__kernel void apply_bandpass_filter(
    __global double2* fft_data,
    const int N,
    const double low_cutoff,
    const double high_cutoff,
    const double rolloff
) {
    int idx = get_global_id(0);
    if (idx < N) {
        double freq = (double)idx / N;
        double gain = 1.0;
        if (freq < low_cutoff) {
            double t = (freq - low_cutoff) / rolloff;
            gain *= 0.5 * (1.0 + tanh(t));
        }
        if (freq > high_cutoff) {
            double t = (high_cutoff - freq) / rolloff;
            gain *= 0.5 * (1.0 + tanh(t));
        }
        double2 data = fft_data[idx];
        fft_data[idx].x = data.x * gain;
        fft_data[idx].y = data.y * gain;
    }
}

__kernel void normalize_data(
    __global double* data,
    const int N,
    const double scale
) {
    int idx = get_global_id(0);
    if (idx < N) {
        data[idx] *= scale;
    }
}

__kernel void normalize_complex_data(
    __global double2* data,
    const int N,
    const double scale
) {
    int idx = get_global_id(0);
    if (idx < N) {
        data[idx].x *= scale;
        data[idx].y *= scale;
    }
}
)";

FilterKernel::FilterKernel(OpenCLContext& ctx) 
    : ctx_(ctx), bufferSize_(0) {
    loadKernels();
}

void FilterKernel::loadKernels() {
    program_ = KernelLoader::loadFromSource(ctx_, filterKernelSource);
    
    try {
        applyFilterKernel_ = cl::Kernel(program_, "apply_filter_freq_domain");
        applyComplexFilterKernel_ = cl::Kernel(program_, "apply_complex_filter_freq_domain");
        applyMultipleFiltersKernel_ = cl::Kernel(program_, "apply_multiple_filters");
        applyBandpassKernel_ = cl::Kernel(program_, "apply_bandpass_filter");
        normalizeKernel_ = cl::Kernel(program_, "normalize_data");
        normalizeComplexKernel_ = cl::Kernel(program_, "normalize_complex_data");
    } catch (const cl::Error& e) {
        throw std::runtime_error(std::string("Failed to create filter kernels: ") + e.what());
    }
}

void FilterKernel::allocateBuffers(size_t size) {
    if (size <= bufferSize_) return;
    
    tempFFTBuffer_ = ctx_.createBuffer(size * sizeof(std::complex<double>), CL_MEM_READ_WRITE);
    tempFilterBuffer_ = ctx_.createBuffer(size * sizeof(double), CL_MEM_READ_ONLY);
    
    bufferSize_ = size;
}

std::vector<std::complex<double>> FilterKernel::applyFilter(
    const std::vector<std::complex<double>>& fftData,
    const std::vector<double>& /* filterResponse */
) {
    // TODO: Implement host-device transfer and kernel execution
    return fftData; // Placeholder
}

void FilterKernel::applyFilterGPU(
    cl::Buffer& /* fftDataBuffer */,
    cl::Buffer& /* filterBuffer */,
    size_t /* N */
) {
    // TODO: Implement kernel execution
}

void FilterKernel::applyComplexFilterGPU(
    cl::Buffer& /* fftDataBuffer */,
    cl::Buffer& /* filterBuffer */,
    size_t /* N */
) {
    // TODO: Implement kernel execution
}

void FilterKernel::applyMultipleFiltersGPU(
    cl::Buffer& /* fftDataBuffer */,
    cl::Buffer& /* filtersBuffer */,
    size_t /* numFilters */,
    size_t /* N */
) {
    // TODO: Implement kernel execution
}

void FilterKernel::applyBandpassFilterGPU(
    cl::Buffer& /* fftDataBuffer */,
    size_t /* N */,
    double /* lowCutoff */,
    double /* highCutoff */,
    double /* rolloff */
) {
    // TODO: Implement kernel execution
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_OPENCL
