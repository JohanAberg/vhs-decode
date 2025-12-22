/**
 * @file benchmark_gpu_kernels.cpp
 * @brief GPU kernel performance benchmarking tool
 * 
 * Tests individual GPU kernels (envelope detection, phase unwrap, filtering)
 * and compares CPU vs GPU performance.
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>
#include <complex>
#include <stdexcept>

#ifdef HAVE_OPENCL
#include "vhsdecode/opencl_context.hpp"
#endif

using namespace vhsdecode;

// Generate test data
std::vector<std::complex<float>> generateComplexSignal(size_t size) {
    std::vector<std::complex<float>> signal(size);
    std::mt19937 gen(42);
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    
    for (size_t i = 0; i < size; ++i) {
        float real = static_cast<float>(dis(gen));
        float imag = static_cast<float>(dis(gen));
        signal[i] = std::complex<float>(real, imag);
    }
    
    return signal;
}

std::vector<float> generateRealSignal(size_t size) {
    std::vector<float> signal(size);
    std::mt19937 gen(42);
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    
    for (size_t i = 0; i < size; ++i) {
        signal[i] = static_cast<float>(dis(gen));
    }
    
    return signal;
}

// CPU envelope detection
std::vector<float> envelopeDetectCPU(const std::vector<std::complex<float>>& signal) {
    std::vector<float> envelope(signal.size());
    for (size_t i = 0; i < signal.size(); ++i) {
        envelope[i] = std::abs(signal[i]);
    }
    return envelope;
}

// CPU phase extraction and unwrap
std::vector<float> phaseUnwrapCPU(const std::vector<std::complex<float>>& signal) {
    if (signal.empty()) return {};
    
    std::vector<float> phase(signal.size());
    phase[0] = std::arg(signal[0]);
    
    for (size_t i = 1; i < signal.size(); ++i) {
        float phase_i = std::arg(signal[i]);
        float phase_prev = phase[i - 1];
        
        // Unwrap phase discontinuities
        float diff = phase_i - phase_prev;
        if (diff > M_PI) {
            phase_i -= 2 * M_PI;
        } else if (diff < -M_PI) {
            phase_i += 2 * M_PI;
        }
        
        phase[i] = phase_i;
    }
    
    return phase;
}

void benchmarkEnvelopeDetection(size_t size = 32768, int iterations = 100) {
    std::cout << "\n=== Envelope Detection Benchmark ===" << std::endl;
    std::cout << "Block size: " << size << " samples" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    
    auto signal = generateComplexSignal(size);
    
    // CPU benchmark
    auto cpuStart = std::chrono::high_resolution_clock::now();
    std::vector<float> envelope;
    for (int i = 0; i < iterations; ++i) {
        envelope = envelopeDetectCPU(signal);
    }
    auto cpuEnd = std::chrono::high_resolution_clock::now();
    double cpuTimeMs = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
    
    std::cout << "CPU envelope detection: " << std::fixed << std::setprecision(2) << cpuTimeMs << " ms" << std::endl;
    std::cout << "  Per iteration: " << cpuTimeMs / iterations << " ms" << std::endl;
    
#ifdef HAVE_OPENCL
    try {
        gpu::OpenCLContext gpuCtx;
        std::cout << "\nGPU Device: " << gpuCtx.getDeviceName() << std::endl;
        std::cout << "Platform: " << gpuCtx.getPlatformName() << std::endl;
        
        // GPU envelope detection would go here
        // For now, we just show GPU is available
        std::cout << "\nGPU envelope kernel available but not yet implemented in this benchmark" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "GPU not available: " << e.what() << std::endl;
    }
#else
    std::cout << "\nOpenCL not available - GPU benchmarks not compiled" << std::endl;
#endif
}

void benchmarkPhaseUnwrap(size_t size = 32768, int iterations = 100) {
    std::cout << "\n=== Phase Unwrap Benchmark ===" << std::endl;
    std::cout << "Block size: " << size << " samples" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    
    auto signal = generateComplexSignal(size);
    
    // CPU benchmark
    auto cpuStart = std::chrono::high_resolution_clock::now();
    std::vector<float> phase;
    for (int i = 0; i < iterations; ++i) {
        phase = phaseUnwrapCPU(signal);
    }
    auto cpuEnd = std::chrono::high_resolution_clock::now();
    double cpuTimeMs = std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count();
    
    std::cout << "CPU phase unwrap: " << std::fixed << std::setprecision(2) << cpuTimeMs << " ms" << std::endl;
    std::cout << "  Per iteration: " << cpuTimeMs / iterations << " ms" << std::endl;
    
#ifdef HAVE_OPENCL
    try {
        gpu::OpenCLContext gpuCtx;
        std::cout << "\nGPU Device: " << gpuCtx.getDeviceName() << std::endl;
        
        // GPU phase unwrap would go here
        std::cout << "\nGPU phase unwrap kernel available but not yet implemented in this benchmark" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "GPU not available: " << e.what() << std::endl;
    }
#else
    std::cout << "\nOpenCL not available - GPU benchmarks not compiled" << std::endl;
#endif
}

void benchmarkGPUAvailability() {
    std::cout << "\n=== GPU Availability Check ===" << std::endl;
    
#ifdef HAVE_OPENCL
    std::cout << "OpenCL support: Compiled in" << std::endl;
    
    try {
        gpu::OpenCLContext gpuCtx;
        std::cout << "\nGPU Device Found!" << std::endl;
        std::cout << "  Device: " << gpuCtx.getDeviceName() << std::endl;
        std::cout << "  Platform: " << gpuCtx.getPlatformName() << std::endl;
        std::cout << "  Type: " << gpuCtx.getDeviceTypeString() << std::endl;
        std::cout << "  Compute Units: " << gpuCtx.getComputeUnits() << std::endl;
        std::cout << "  Max Clock: " << gpuCtx.getMaxClockFrequency() << " MHz" << std::endl;
        std::cout << "  Global Memory: " << (gpuCtx.getGlobalMemSize() / (1024*1024)) << " MB" << std::endl;
        std::cout << "  Max Workgroup Size: " << gpuCtx.getMaxWorkGroupSize() << std::endl;
        
        return;
    } catch (const std::exception& e) {
        std::cout << "GPU initialization failed: " << e.what() << std::endl;
    }
#else
    std::cout << "OpenCL support: NOT compiled" << std::endl;
    std::cout << "\nTo enable GPU support:" << std::endl;
    std::cout << "  1. Install OpenCL development headers" << std::endl;
    std::cout << "  2. Rebuild with: cmake -B build -DENABLE_GPU=ON" << std::endl;
    std::cout << "\nTo enable GPU-accelerated FFT:" << std::endl;
    std::cout << "  1. Install clFFT development libraries" << std::endl;
    std::cout << "  2. Rebuild with: cmake -B build -DENABLE_GPU=ON -DUSE_CLFFT=ON" << std::endl;
#endif
}

int main(int /* argc */, char* /* argv */[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "    VHS-Decode GPU Kernel Benchmarks" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        benchmarkGPUAvailability();
        benchmarkEnvelopeDetection(32768, 100);
        benchmarkPhaseUnwrap(32768, 100);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Benchmarks Complete!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
