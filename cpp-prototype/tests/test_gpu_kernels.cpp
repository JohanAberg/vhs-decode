/**
 * @file test_gpu_kernels.cpp
 * @brief Basic tests for GPU kernels (Phase 4.3)
 * 
 * Tests verify that:
 * 1. GPU kernels can be instantiated
 * 2. OpenCL context can be created
 * 3. Kernels can be loaded and compiled
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>

#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include "vhsdecode/gpu/phase_unwrap_kernel.hpp"
#include "vhsdecode/gpu/envelope_kernel.hpp"
#include "vhsdecode/gpu/filter_kernel.hpp"

using namespace vhsdecode::gpu;

// Test result structure
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMsg;
};

// Test 1: OpenCL context creation
TestResult testOpenCLContext() {
    TestResult result{"OpenCL Context Creation", false, ""};
    
    try {
        OpenCLContext ctx;
        std::cout << "✓ OpenCL context created successfully" << std::endl;
        std::cout << "  Device: " << ctx.getDeviceName() << std::endl;
        std::cout << "  Platform: " << ctx.getPlatformName() << std::endl;
        result.passed = true;
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ OpenCL context creation failed: " << e.what() << std::endl;
    }
    
    return result;
}

// Test 2: PhaseUnwrapKernel instantiation
TestResult testPhaseUnwrapKernel() {
    TestResult result{"PhaseUnwrapKernel Instantiation", false, ""};
    
    try {
        OpenCLContext ctx;
        PhaseUnwrapKernel kernel(ctx);
        std::cout << "✓ PhaseUnwrapKernel instantiated successfully" << std::endl;
        result.passed = true;
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ PhaseUnwrapKernel instantiation failed: " << e.what() << std::endl;
    }
    
    return result;
}

// Test 3: EnvelopeKernel instantiation
TestResult testEnvelopeKernel() {
    TestResult result{"EnvelopeKernel Instantiation", false, ""};
    
    try {
        OpenCLContext ctx;
        EnvelopeKernel kernel(ctx);
        std::cout << "✓ EnvelopeKernel instantiated successfully" << std::endl;
        result.passed = true;
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ EnvelopeKernel instantiation failed: " << e.what() << std::endl;
    }
    
    return result;
}

// Test 4: FilterKernel instantiation
TestResult testFilterKernel() {
    TestResult result{"FilterKernel Instantiation", false, ""};
    
    try {
        OpenCLContext ctx;
        FilterKernel kernel(ctx);
        std::cout << "✓ FilterKernel instantiated successfully" << std::endl;
        result.passed = true;
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ FilterKernel instantiation failed: " << e.what() << std::endl;
    }
    
    return result;
}

// Test 5: PhaseUnwrapKernel basic functionality
TestResult testPhaseUnwrapBasic() {
    TestResult result{"PhaseUnwrapKernel Basic Functionality", false, ""};
    
    try {
        OpenCLContext ctx;
        PhaseUnwrapKernel kernel(ctx);
        
        // Create test data: simple sawtooth wave (wraps at ±π)
        const size_t N = 1024;
        std::vector<double> phase(N);
        for (size_t i = 0; i < N; ++i) {
            // Sawtooth that wraps multiple times
            phase[i] = std::fmod(i * 0.1, 2.0 * M_PI) - M_PI;
        }
        
        // Unwrap phase
        auto unwrapped = kernel.unwrap(phase);
        
        // Verify output size
        if (unwrapped.size() != N) {
            result.errorMsg = "Output size mismatch";
            std::cout << "✗ PhaseUnwrapKernel output size incorrect" << std::endl;
            return result;
        }
        
        std::cout << "✓ PhaseUnwrapKernel basic functionality passed" << std::endl;
        std::cout << "  Input range: [" << phase[0] << ", " << phase[N-1] << "]" << std::endl;
        std::cout << "  Output range: [" << unwrapped[0] << ", " << unwrapped[N-1] << "]" << std::endl;
        result.passed = true;
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ PhaseUnwrapKernel basic functionality failed: " << e.what() << std::endl;
    }
    
    return result;
}

// Test 6: EnvelopeKernel basic functionality
TestResult testEnvelopeBasic() {
    TestResult result{"EnvelopeKernel Basic Functionality", false, ""};
    
    try {
        OpenCLContext ctx;
        EnvelopeKernel kernel(ctx);
        
        // Create test data: simple AM signal (analytic)
        const size_t N = 1024;
        std::vector<std::complex<double>> analytic(N);
        for (size_t i = 0; i < N; ++i) {
            double t = static_cast<double>(i) / N;
            // AM envelope: 1 + 0.5 * cos(2π * 5 * t)
            double env = 1.0 + 0.5 * std::cos(2.0 * M_PI * 5.0 * t);
            // Carrier at 100 Hz
            double carrier = std::cos(2.0 * M_PI * 100.0 * t);
            analytic[i] = std::complex<double>(env * carrier, env * std::sin(2.0 * M_PI * 100.0 * t));
        }
        
        // Detect envelope
        auto envelope = kernel.detect(analytic);
        
        // Verify output size
        if (envelope.size() != N) {
            result.errorMsg = "Output size mismatch";
            std::cout << "✗ EnvelopeKernel output size incorrect" << std::endl;
            return result;
        }
        
        // Check that envelope is reasonable (between 0.5 and 1.5)
        bool valid = true;
        for (const auto& val : envelope) {
            if (val < 0.4 || val > 1.6) {
                valid = false;
                break;
            }
        }
        
        if (!valid) {
            result.errorMsg = "Envelope values out of expected range";
            std::cout << "✗ EnvelopeKernel envelope values out of range" << std::endl;
            return result;
        }
        
        std::cout << "✓ EnvelopeKernel basic functionality passed" << std::endl;
        std::cout << "  Envelope range: [" << envelope[0] << ", " << envelope[N/2] << "]" << std::endl;
        result.passed = true;
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ EnvelopeKernel basic functionality failed: " << e.what() << std::endl;
    }
    
    return result;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "GPU Kernel Tests (Phase 4.3)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    std::vector<TestResult> results;
    
    // Run all tests
    results.push_back(testOpenCLContext());
    results.push_back(testPhaseUnwrapKernel());
    results.push_back(testEnvelopeKernel());
    results.push_back(testFilterKernel());
    results.push_back(testPhaseUnwrapBasic());
    results.push_back(testEnvelopeBasic());
    
    // Summary
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    
    int passed = 0;
    int total = results.size();
    
    for (const auto& result : results) {
        std::string status = result.passed ? "PASS" : "FAIL";
        std::cout << "[" << status << "] " << result.testName;
        if (!result.passed && !result.errorMsg.empty()) {
            std::cout << " - " << result.errorMsg;
        }
        std::cout << std::endl;
        if (result.passed) passed++;
    }
    
    std::cout << std::endl;
    std::cout << "Result: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (passed == total) ? 0 : 1;
}

#else // !HAVE_OPENCL

int main() {
    std::cout << "OpenCL support not enabled - skipping GPU kernel tests" << std::endl;
    return 0;
}

#endif // HAVE_OPENCL
