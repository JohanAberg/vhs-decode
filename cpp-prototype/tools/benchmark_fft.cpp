/**
 * @file benchmark_fft.cpp
 * @brief FFT performance benchmarking tool
 * 
 * Compares prototype FFT implementation vs FFTW3 vs clFFT GPU across different block sizes.
 * Validates numerical accuracy and measures performance improvements.
 */

#include "vhsdecode/fft_engine.hpp"
#include "vhsdecode/fftw_engine.hpp"
#ifdef HAVE_CLFFT
#include "vhsdecode/clfft_engine.hpp"
#include "vhsdecode/opencl_context.hpp"
#endif
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>

using namespace vhsdecode::rf;

struct BenchmarkResult {
    size_t blockSize;
    double prototypeTimeMs;
    double fftwTimeMs;
    double gpuTimeMs;
    double speedupFFTW;
    double speedupGPU;
    double maxError;
    bool passed;
};

// Generate test signal (sum of sinusoids)
std::vector<float> generateTestSignal(size_t size) {
    std::vector<float> signal(size);
    const float pi = 3.14159265358979323846f;
    
    for (size_t i = 0; i < size; ++i) {
        float t = static_cast<float>(i) / size;
        // Mix of frequencies
        signal[i] = 
            std::sin(2 * pi * 5 * t) +
            0.5f * std::sin(2 * pi * 10 * t) +
            0.25f * std::sin(2 * pi * 20 * t);
    }
    
    return signal;
}

// Convert float vector to double vector
std::vector<double> toDouble(const std::vector<float>& input) {
    return std::vector<double>(input.begin(), input.end());
}

// Convert double vector to float vector
std::vector<float> toFloat(const std::vector<double>& input) {
    return std::vector<float>(input.begin(), input.end());
}

// Calculate maximum absolute error
float calculateMaxError(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return std::numeric_limits<float>::max();
    
    float maxErr = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float err = std::abs(a[i] - b[i]);
        maxErr = std::max(maxErr, err);
    }
    return maxErr;
}

BenchmarkResult benchmarkBlockSize(size_t blockSize, int iterations = 10
#ifdef HAVE_CLFFT
    , vhsdecode::gpu::OpenCLContext* gpuCtx = nullptr
#endif
) {
    BenchmarkResult result;
    result.blockSize = blockSize;
    result.passed = true;
    result.gpuTimeMs = 0.0;
    result.speedupGPU = 0.0;
    
    std::cout << "\nBenchmarking block size: " << blockSize << std::endl;
    
    // Generate test signal
    auto signal = generateTestSignal(blockSize);
    
    // Benchmark prototype FFT
    try {
        FFTEngine prototypeEngine(blockSize, false);
        
        auto startProto = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto fft = prototypeEngine.forwardFFT(signal);
            auto reconstructed = prototypeEngine.inverseFFT(fft);
        }
        auto endProto = std::chrono::high_resolution_clock::now();
        
        result.prototypeTimeMs = 
            std::chrono::duration<double, std::milli>(endProto - startProto).count() / iterations;
        
        std::cout << "  Prototype FFT: " << std::fixed << std::setprecision(2) 
                  << result.prototypeTimeMs << " ms" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Prototype FFT: FAILED (" << e.what() << ")" << std::endl;
        result.prototypeTimeMs = 0.0;
        result.passed = false;
    }
    
    // Benchmark FFTW3
    try {
        FFTWEngine fftwEngine(blockSize, false);
        std::vector<double> signalDouble = toDouble(signal);
        
        auto startFFTW = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto fft = fftwEngine.forwardFFT(signalDouble);
            auto reconstructed = fftwEngine.inverseFFT(fft);
        }
        auto endFFTW = std::chrono::high_resolution_clock::now();
        
        result.fftwTimeMs = 
            std::chrono::duration<double, std::milli>(endFFTW - startFFTW).count() / iterations;
        
        std::cout << "  FFTW3 FFT:     " << std::fixed << std::setprecision(2) 
                  << result.fftwTimeMs << " ms" << std::endl;
        
        // Test numerical accuracy
        auto fftResult = fftwEngine.forwardFFT(signalDouble);
        auto reconstructed = fftwEngine.inverseFFT(fftResult);
        result.maxError = calculateMaxError(signal, toFloat(reconstructed));
        
        std::cout << "  Max error:     " << std::scientific << std::setprecision(2) 
                  << result.maxError << std::endl;
        
        if (result.maxError > 1e-6) {
            std::cout << "  WARNING: Numerical accuracy below threshold!" << std::endl;
            result.passed = false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FFTW3 FFT: FAILED (" << e.what() << ")" << std::endl;
        result.fftwTimeMs = 0.0;
        result.passed = false;
    }
    
#ifdef HAVE_CLFFT
    // Benchmark GPU FFT
    if (gpuCtx != nullptr) {
        try {
            vhsdecode::gpu::CLFFTEngine gpuEngine(*gpuCtx, blockSize, true);
            std::vector<double> signalDouble = toDouble(signal);
            
            auto startGPU = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                auto fft = gpuEngine.forwardFFT(signalDouble);
                auto reconstructed = gpuEngine.inverseFFT(fft);
            }
            auto endGPU = std::chrono::high_resolution_clock::now();
            
            result.gpuTimeMs = 
                std::chrono::duration<double, std::milli>(endGPU - startGPU).count() / iterations;
            
            std::cout << "  GPU FFT (clFFT): " << std::fixed << std::setprecision(2) 
                      << result.gpuTimeMs << " ms" << std::endl;
            
            // Test numerical accuracy vs FFTW3
            auto gpuFftResult = gpuEngine.forwardFFT(signalDouble);
            auto gpuReconstructed = gpuEngine.inverseFFT(gpuFftResult);
            double gpuError = calculateMaxError(signal, toFloat(gpuReconstructed));
            
            std::cout << "  GPU Max error: " << std::scientific << std::setprecision(2) 
                      << gpuError << std::endl;
            
            if (gpuError > 1e-6) {
                std::cout << "  WARNING: GPU numerical accuracy below threshold!" << std::endl;
                result.passed = false;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  GPU FFT: FAILED (" << e.what() << ")" << std::endl;
            result.gpuTimeMs = 0.0;
        }
    }
#endif
    
    // Calculate speedup
    if (result.prototypeTimeMs > 0 && result.fftwTimeMs > 0) {
        result.speedupFFTW = result.prototypeTimeMs / result.fftwTimeMs;
        std::cout << "  FFTW3 Speedup: " << std::fixed << std::setprecision(1) 
                  << result.speedupFFTW << "x" << std::endl;
    } else {
        result.speedupFFTW = 0.0;
    }
    
    if (result.fftwTimeMs > 0 && result.gpuTimeMs > 0) {
        result.speedupGPU = result.fftwTimeMs / result.gpuTimeMs;
        std::cout << "  GPU vs FFTW3:  " << std::fixed << std::setprecision(1) 
                  << result.speedupGPU << "x" << std::endl;
    } else {
        result.speedupGPU = 0.0;
    }
    
    return result;
}

void printSummaryTable(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n\n";
    std::cout << "============================================================" << std::endl;
    std::cout << "                  Benchmark Summary" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::setw(12) << "Block Size"
              << std::setw(15) << "Prototype (ms)"
              << std::setw(15) << "FFTW3 (ms)"
              << std::setw(15) << "GPU (ms)"
              << std::setw(12) << "GPU Speedup"
              << std::setw(10) << "Status"
              << std::endl;
    
    std::cout << std::string(85, '-') << std::endl;
    
    for (const auto& result : results) {
        std::cout << std::setw(12) << result.blockSize
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.prototypeTimeMs
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.fftwTimeMs
                  << std::setw(15) << std::fixed << std::setprecision(2) 
                  << (result.gpuTimeMs > 0 ? std::to_string(result.gpuTimeMs) : "N/A")
                  << std::setw(12) << std::fixed << std::setprecision(1) 
                  << (result.speedupGPU > 0 ? std::to_string(result.speedupGPU) + "x" : "N/A")
                  << std::setw(10) << (result.passed ? "PASS" : "FAIL")
                  << std::endl;
    }
    
    std::cout << std::endl;
    
    // Calculate average GPU speedup
    double totalSpeedupGPU = 0.0;
    int validGPUResults = 0;
    for (const auto& result : results) {
        if (result.speedupGPU > 0) {
            totalSpeedupGPU += result.speedupGPU;
            validGPUResults++;
        }
    }
    
    if (validGPUResults > 0) {
        double avgSpeedupGPU = totalSpeedupGPU / validGPUResults;
        std::cout << "Average GPU Speedup (vs FFTW3): " << std::fixed << std::setprecision(1) 
                  << avgSpeedupGPU << "x" << std::endl;
    }
    
    std::cout << "============================================================" << std::endl;
}

int main(int /* argc */, char* /* argv */[]) {
    std::cout << "============================================================" << std::endl;
    std::cout << "          VHS-Decode FFT Benchmark Tool" << std::endl;
    std::cout << "   Prototype FFT vs FFTW3 vs GPU (clFFT) Comparison" << std::endl;
    std::cout << "============================================================" << std::endl;
    
#ifdef HAVE_CLFFT
    // Initialize GPU context if available
    vhsdecode::gpu::OpenCLContext* gpuCtx = nullptr;
    try {
        gpuCtx = new vhsdecode::gpu::OpenCLContext();
        std::cout << "\nGPU Acceleration Available:" << std::endl;
        std::cout << "  Device: " << gpuCtx->getDeviceName() << std::endl;
        std::cout << "  Platform: " << gpuCtx->getPlatformName() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "\nGPU Acceleration NOT Available: " << e.what() << std::endl;
        std::cout << "Running CPU-only benchmarks..." << std::endl;
    }
#else
    std::cout << "\nclFFT not available - GPU benchmarks skipped" << std::endl;
#endif
    
    // Test block sizes (powers of 2)
    std::vector<size_t> blockSizes = {1024, 4096, 16384, 32768, 65536};
    
    // Run benchmarks
    std::vector<BenchmarkResult> results;
    for (size_t size : blockSizes) {
#ifdef HAVE_CLFFT
        results.push_back(benchmarkBlockSize(size, 10, gpuCtx));
#else
        results.push_back(benchmarkBlockSize(size, 10));
#endif
    }
    
    // Print summary
    printSummaryTable(results);
    
#ifdef HAVE_CLFFT
    // Cleanup GPU context
    if (gpuCtx != nullptr) {
        delete gpuCtx;
    }
#endif
    
    // Check if all tests passed
    bool allPassed = std::all_of(results.begin(), results.end(),
                                  [](const BenchmarkResult& r) { return r.passed; });
    
    if (allPassed) {
        std::cout << "\n✓ All benchmarks PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ Some benchmarks FAILED!" << std::endl;
        return 1;
    }
}
