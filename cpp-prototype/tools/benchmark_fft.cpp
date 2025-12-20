/**
 * @file benchmark_fft.cpp
 * @brief FFT performance benchmarking tool
 * 
 * Compares prototype FFT implementation vs FFTW3 across different block sizes.
 * Validates numerical accuracy and measures performance improvements.
 */

#include "vhsdecode/fft_engine.hpp"
#include "vhsdecode/fftw_engine.hpp"
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
    double speedup;
    double maxError;
    bool passed;
};

// Generate test signal (sum of sinusoids)
std::vector<double> generateTestSignal(size_t size) {
    std::vector<double> signal(size);
    const double pi = 3.14159265358979323846;
    
    for (size_t i = 0; i < size; ++i) {
        double t = static_cast<double>(i) / size;
        // Mix of frequencies
        signal[i] = 
            std::sin(2 * pi * 5 * t) +
            0.5 * std::sin(2 * pi * 10 * t) +
            0.25 * std::sin(2 * pi * 20 * t);
    }
    
    return signal;
}

// Calculate maximum absolute error
double calculateMaxError(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return std::numeric_limits<double>::max();
    
    double maxErr = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        double err = std::abs(a[i] - b[i]);
        maxErr = std::max(maxErr, err);
    }
    return maxErr;
}

BenchmarkResult benchmarkBlockSize(size_t blockSize, int iterations = 10) {
    BenchmarkResult result;
    result.blockSize = blockSize;
    result.passed = true;
    
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
        
        auto startFFTW = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto fft = fftwEngine.forwardFFT(signal);
            auto reconstructed = fftwEngine.inverseFFT(fft);
        }
        auto endFFTW = std::chrono::high_resolution_clock::now();
        
        result.fftwTimeMs = 
            std::chrono::duration<double, std::milli>(endFFTW - startFFTW).count() / iterations;
        
        std::cout << "  FFTW3 FFT:     " << std::fixed << std::setprecision(2) 
                  << result.fftwTimeMs << " ms" << std::endl;
        
        // Test numerical accuracy
        auto fftResult = fftwEngine.forwardFFT(signal);
        auto reconstructed = fftwEngine.inverseFFT(fftResult);
        result.maxError = calculateMaxError(signal, reconstructed);
        
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
    
    // Calculate speedup
    if (result.prototypeTimeMs > 0 && result.fftwTimeMs > 0) {
        result.speedup = result.prototypeTimeMs / result.fftwTimeMs;
        std::cout << "  Speedup:       " << std::fixed << std::setprecision(1) 
                  << result.speedup << "x" << std::endl;
    } else {
        result.speedup = 0.0;
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
              << std::setw(12) << "Speedup"
              << std::setw(15) << "Max Error"
              << std::setw(10) << "Status"
              << std::endl;
    
    std::cout << std::string(78, '-') << std::endl;
    
    for (const auto& result : results) {
        std::cout << std::setw(12) << result.blockSize
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.prototypeTimeMs
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.fftwTimeMs
                  << std::setw(12) << std::fixed << std::setprecision(1) << result.speedup << "x"
                  << std::setw(15) << std::scientific << std::setprecision(2) << result.maxError
                  << std::setw(10) << (result.passed ? "PASS" : "FAIL")
                  << std::endl;
    }
    
    std::cout << std::endl;
    
    // Calculate average speedup
    double totalSpeedup = 0.0;
    int validResults = 0;
    for (const auto& result : results) {
        if (result.speedup > 0) {
            totalSpeedup += result.speedup;
            validResults++;
        }
    }
    
    if (validResults > 0) {
        double avgSpeedup = totalSpeedup / validResults;
        std::cout << "Average Speedup: " << std::fixed << std::setprecision(1) 
                  << avgSpeedup << "x" << std::endl;
    }
    
    std::cout << "============================================================" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "============================================================" << std::endl;
    std::cout << "          VHS-Decode FFT Benchmark Tool" << std::endl;
    std::cout << "       Prototype FFT vs FFTW3 Comparison" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    // Test block sizes (powers of 2)
    std::vector<size_t> blockSizes = {1024, 4096, 16384, 32768, 65536};
    
    // Run benchmarks
    std::vector<BenchmarkResult> results;
    for (size_t size : blockSizes) {
        results.push_back(benchmarkBlockSize(size, 10));
    }
    
    // Print summary
    printSummaryTable(results);
    
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
