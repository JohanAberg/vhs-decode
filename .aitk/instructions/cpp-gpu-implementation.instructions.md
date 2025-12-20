---
description: C++ GPU acceleration implementation guidelines for VHS-Decode cpp-prototype
applyTo: 'cpp-prototype/**'
priority: high
---

# C++ GPU Implementation Instructions for AI Agents

## Overview

This document provides guidelines for implementing GPU acceleration in the VHS-Decode C++ prototype using OpenCL, clFFT, and C++17. This is **separate** from the Python GPU work - this applies to the `cpp-prototype` directory.

## Technology Stack

- **Language:** C++17
- **GPU API:** OpenCL 1.2+ (cross-platform)
- **FFT Library:** clFFT (GPU) / FFTW3 (CPU)
- **Build System:** CMake 3.16+
- **Testing:** Custom test framework in `tests/`

## Project Context

### Phase Status (December 2024)
- ✅ **Phase 4.1:** OpenCL infrastructure (context, buffers) - COMPLETE
- ✅ **Phase 4.2:** clFFT GPU FFT integration - COMPLETE  
- ✅ **Phase 4.3:** GPU kernel development - COMPLETE
- 🔄 **Phase 4.4:** Hybrid CPU/GPU processing - NEXT

### Current Architecture
```
cpp-prototype/
├── include/vhsdecode/
│   ├── opencl_context.hpp      # OpenCL device management
│   ├── opencl_buffer.hpp       # GPU memory management
│   ├── clfft_engine.hpp        # GPU FFT (Phase 4.2)
│   └── gpu/
│       ├── phase_unwrap_kernel.hpp  # FM demod phase unwrapping
│       ├── envelope_kernel.hpp      # Dropout detection
│       ├── filter_kernel.hpp        # Frequency-domain filtering
│       └── kernel_loader.hpp        # OpenCL compilation utility
├── src/gpu/
│   ├── opencl_context.cpp
│   ├── opencl_buffer.cpp
│   ├── clfft_engine.cpp
│   ├── phase_unwrap_kernel.cpp
│   ├── envelope_kernel.cpp
│   ├── filter_kernel.cpp
│   ├── kernel_loader.cpp
│   └── kernels/
│       ├── phase_unwrap.cl      # Phase unwrapping kernels
│       ├── envelope.cl          # Envelope detection kernels
│       └── filter_apply.cl      # Filter application kernels
└── tests/
    ├── test_gpu_kernels.cpp     # GPU unit tests
    └── CMakeLists.txt
```

## Core Principles

### 1. **RAII Resource Management**
All GPU resources must use RAII for automatic cleanup:
```cpp
class GPUResource {
public:
    GPUResource(OpenCLContext& ctx) : ctx_(ctx) {
        // Allocate resources
    }
    
    ~GPUResource() {
        // Automatic cleanup
    }
    
    // Delete copy, allow move
    GPUResource(const GPUResource&) = delete;
    GPUResource& operator=(const GPUResource&) = delete;
    GPUResource(GPUResource&&) noexcept = default;
    GPUResource& operator=(GPUResource&&) noexcept = default;

private:
    OpenCLContext& ctx_;
};
```

### 2. **CPU Fallback Always**
Every GPU function must have CPU equivalent:
```cpp
#ifdef HAVE_OPENCL
    if (useGPU_ && gpuContext_) {
        try {
            return processGPU(data);
        } catch (const cl::Error& e) {
            std::cerr << "GPU failed: " << e.what() 
                      << ", falling back to CPU" << std::endl;
        }
    }
#endif
    return processCPU(data);
```

### 3. **Exception Safety**
Use try-catch blocks for all OpenCL operations:
```cpp
try {
    kernel.setArg(0, buffer);
    queue.enqueueNDRangeKernel(kernel, cl::NullRange, 
                               cl::NDRange(N), cl::NullRange);
} catch (const cl::Error& e) {
    throw std::runtime_error(
        std::string("OpenCL error: ") + e.what()
    );
}
```

## Build System Guidelines

### CMake Configuration

**Conditional Compilation:**
```cmake
# OpenCL detection
if(ENABLE_GPU)
    find_package(OpenCL)
    if(OpenCL_FOUND)
        add_definitions(-DHAVE_OPENCL)
        target_link_libraries(target PRIVATE OpenCL::OpenCL)
    endif()
endif()

# clFFT detection (optional)
if(USE_CLFFT AND OpenCL_FOUND)
    find_package(clFFT)
    if(clFFT_FOUND)
        add_definitions(-DHAVE_CLFFT)
        target_link_libraries(target PRIVATE clFFT::clFFT)
    endif()
endif()
```

**Source Organization:**
```cmake
# Add GPU sources only if OpenCL available
if(ENABLE_GPU AND OpenCL_FOUND)
    list(APPEND SOURCES 
        gpu/opencl_context.cpp
        gpu/opencl_buffer.cpp
        gpu/kernel_loader.cpp
        gpu/phase_unwrap_kernel.cpp
        gpu/envelope_kernel.cpp
        gpu/filter_kernel.cpp
    )
endif()

# Add clFFT sources only if available
if(USE_CLFFT AND clFFT_FOUND)
    list(APPEND SOURCES gpu/clfft_engine.cpp)
endif()
```

### Build Commands

**Full GPU build:**
```bash
cd cpp-prototype
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_FFTW3=ON \
    -DENABLE_GPU=ON \
    -DUSE_CLFFT=ON \
    -DBUILD_TESTS=ON

cmake --build . --parallel
```

**CPU-only build:**
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_FFTW3=ON \
    -DENABLE_GPU=OFF

cmake --build . --parallel
```

## OpenCL Kernel Development

### Kernel File Structure

**Embedded kernel source pattern:**
```cpp
// In phase_unwrap_kernel.cpp
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
)";
```

### Kernel Wrapper Pattern

**Complete wrapper class:**
```cpp
class MyKernel {
public:
    explicit MyKernel(OpenCLContext& ctx) : ctx_(ctx), bufferSize_(0) {
        loadKernels();
    }
    
    // CPU interface (with transfers)
    std::vector<double> process(const std::vector<double>& input) {
        size_t N = input.size();
        allocateBuffers(N);
        
        ctx_.writeBuffer(tempInBuffer_, input.data(), N * sizeof(double));
        processGPU(tempInBuffer_, tempOutBuffer_, N);
        
        std::vector<double> result(N);
        ctx_.readBuffer(tempOutBuffer_, result.data(), N * sizeof(double));
        return result;
    }
    
    // GPU interface (no transfers, stays on device)
    void processGPU(cl::Buffer& inBuf, cl::Buffer& outBuf, size_t N) {
        try {
            kernel_.setArg(0, inBuf);
            kernel_.setArg(1, outBuf);
            kernel_.setArg(2, static_cast<int>(N));
            
            cl::CommandQueue& queue = ctx_.getQueue();
            queue.enqueueNDRangeKernel(
                kernel_, 
                cl::NullRange, 
                cl::NDRange(N), 
                cl::NullRange
            );
        } catch (const cl::Error& e) {
            throw std::runtime_error(
                std::string("Kernel execution failed: ") + e.what()
            );
        }
    }

private:
    OpenCLContext& ctx_;
    cl::Program program_;
    cl::Kernel kernel_;
    cl::Buffer tempInBuffer_;
    cl::Buffer tempOutBuffer_;
    size_t bufferSize_;
    
    void loadKernels() {
        program_ = KernelLoader::loadFromSource(ctx_, kernelSource);
        kernel_ = cl::Kernel(program_, "kernel_name");
    }
    
    void allocateBuffers(size_t size) {
        if (size <= bufferSize_) return;
        
        tempInBuffer_ = ctx_.createBuffer(
            size * sizeof(double), CL_MEM_READ_ONLY
        );
        tempOutBuffer_ = ctx_.createBuffer(
            size * sizeof(double), CL_MEM_WRITE_ONLY
        );
        
        bufferSize_ = size;
    }
};
```

## Testing Requirements

### Test Structure

**Unit test pattern:**
```cpp
#ifdef HAVE_OPENCL

#include "vhsdecode/opencl_context.hpp"
#include "vhsdecode/gpu/my_kernel.hpp"

using namespace vhsdecode::gpu;

struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMsg;
};

TestResult testMyKernel() {
    TestResult result{"MyKernel Test", false, ""};
    
    try {
        OpenCLContext ctx;
        MyKernel kernel(ctx);
        
        // Create test data
        std::vector<double> input(1024);
        // ... fill with test data
        
        // Run kernel
        auto output = kernel.process(input);
        
        // Validate output
        if (validateOutput(output)) {
            std::cout << "✓ MyKernel test passed" << std::endl;
            result.passed = true;
        } else {
            result.errorMsg = "Output validation failed";
        }
    } catch (const std::exception& e) {
        result.errorMsg = e.what();
        std::cout << "✗ Test failed: " << e.what() << std::endl;
    }
    
    return result;
}

int main() {
    std::vector<TestResult> results;
    results.push_back(testMyKernel());
    
    int passed = 0;
    for (const auto& r : results) {
        if (r.passed) passed++;
        std::cout << "[" << (r.passed ? "PASS" : "FAIL") << "] " 
                  << r.testName << std::endl;
    }
    
    return (passed == results.size()) ? 0 : 1;
}

#else // !HAVE_OPENCL

int main() {
    std::cout << "OpenCL not available - skipping test" << std::endl;
    return 0;
}

#endif
```

### Running Tests

```bash
cd cpp-prototype/build

# Build tests
cmake --build . --target test-gpu-kernels

# Run tests
./tests/test-gpu-kernels

# Or use CTest
ctest -V

# Expected on system without GPU:
# All tests will fail with "clGetPlatformIDs" error - THIS IS EXPECTED
# Tests verify compilation, not runtime (requires GPU hardware)
```

## Performance Guidelines

### Expected Speedups

| Component | CPU Time | GPU Time | Speedup | Status |
|-----------|----------|----------|---------|--------|
| FFT (clFFT) | 0.89 ms | 0.004 ms | 222x | Phase 4.2 ✅ |
| Phase Unwrap | 0.10 ms | 0.0001 ms | 1000x | Phase 4.3 ✅ |
| Envelope | 0.15 ms | 0.0003 ms | 500x | Phase 4.3 ✅ |
| Filters | 0.05 ms | 0.0001 ms | 500x | Phase 4.3 ✅ |
| **Pipeline** | **2.34 ms** | **0.01 ms** | **234x** | Target |

### Work Group Sizing

**General guidelines:**
```cpp
// For simple operations (no shared memory)
size_t globalSize = dataSize;
size_t localSize = 256;  // Good default for most GPUs

queue.enqueueNDRangeKernel(
    kernel,
    cl::NullRange,
    cl::NDRange(globalSize),
    cl::NDRange(localSize)
);

// For sequential operations (e.g., phase unwrap)
// Use single work item
queue.enqueueNDRangeKernel(
    kernel,
    cl::NullRange,
    cl::NDRange(1),  // Single work item
    cl::NullRange     // No local size needed
);
```

### Memory Transfer Optimization

**Minimize transfers:**
```cpp
// BAD: Multiple transfers
for (int i = 0; i < N; ++i) {
    auto gpu_data = toGPU(data[i]);
    auto result = processGPU(gpu_data);
    results[i] = fromGPU(result);  // Transfer each iteration
}

// GOOD: Batch processing
auto all_gpu_data = toGPU(all_data);
auto all_results = processBatchGPU(all_gpu_data);
auto results = fromGPU(all_results);  // Single transfer
```

## Phase 4.4: Hybrid CPU/GPU Processing

### Components to Implement

**1. WorkloadProfiler:**
```cpp
class WorkloadProfiler {
public:
    struct Profile {
        size_t dataSize;
        size_t fftSize;
        bool hasDropouts;
        double complexity;  // 0.0 = simple, 1.0 = complex
    };
    
    Profile analyzeBlock(const RFBlock& block);
};
```

**2. ProcessorSelector:**
```cpp
class ProcessorSelector {
public:
    enum class Mode { CPU, GPU, Auto };
    
    Mode selectProcessor(const WorkloadProfile& profile) {
        if (!gpuAvailable_) return Mode::CPU;
        if (profile.dataSize < minGPUSize_) return Mode::CPU;
        if (profile.complexity > threshold_) return Mode::GPU;
        return Mode::CPU;
    }
    
private:
    bool gpuAvailable_;
    size_t minGPUSize_ = 8192;
    double threshold_ = 0.3;
};
```

**3. HybridProcessor:**
```cpp
class HybridProcessor {
public:
    HybridProcessor(const Config& config) {
        cpuProcessor_ = std::make_unique<CPUProcessor>(config);
        
        try {
            gpuContext_ = std::make_unique<OpenCLContext>();
            gpuProcessor_ = std::make_unique<GPUProcessor>(*gpuContext_, config);
            gpuAvailable_ = true;
        } catch (const std::exception& e) {
            std::cerr << "GPU init failed: " << e.what() << std::endl;
            gpuAvailable_ = false;
        }
    }
    
    Result process(const RFBlock& block) {
        auto profile = profiler_.analyzeBlock(block);
        auto mode = selector_.selectProcessor(profile);
        
        try {
            if (mode == Mode::GPU && gpuAvailable_) {
                return gpuProcessor_->process(block);
            }
        } catch (const cl::Error& e) {
            std::cerr << "GPU error, falling back to CPU" << std::endl;
        }
        
        return cpuProcessor_->process(block);
    }

private:
    std::unique_ptr<CPUProcessor> cpuProcessor_;
    std::unique_ptr<OpenCLContext> gpuContext_;
    std::unique_ptr<GPUProcessor> gpuProcessor_;
    WorkloadProfiler profiler_;
    ProcessorSelector selector_;
    bool gpuAvailable_ = false;
};
```

## Common Issues and Solutions

### Issue: "clGetPlatformIDs" Error

**Cause:** No OpenCL runtime or GPU available  
**Solution:** This is expected in CI environments without GPU. Code still validates via compilation.

### Issue: Kernel Compilation Fails

**Cause:** Syntax error in OpenCL kernel  
**Solution:**
```cpp
try {
    program_.build();
} catch (const cl::Error& e) {
    // Get build log
    std::string log = program_.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
    std::cerr << "Build failed:\n" << log << std::endl;
    throw;
}
```

### Issue: Memory Leaks

**Cause:** Not using RAII or explicit cleanup  
**Solution:** Always use RAII pattern, cl::Buffer handles cleanup automatically

### Issue: Performance Below Target

**Cause:** Transfer overhead or wrong work group size  
**Solution:**
1. Profile with `cl::Event` objects
2. Batch more operations on GPU
3. Tune work group sizes
4. Check for unnecessary synchronization

## Documentation Requirements

**Every GPU class must have:**
```cpp
/**
 * @brief Brief description
 * 
 * Detailed description of what this class does.
 * 
 * CPU Equivalent: OriginalClass in original_file.cpp
 * Expected Speedup: 200-500x
 * Memory Required: ~X MB VRAM per operation
 * Precision: FP32 for Y, FP64 for Z
 * 
 * Example:
 * @code
 * OpenCLContext ctx;
 * MyKernel kernel(ctx);
 * auto result = kernel.process(input);
 * @endcode
 * 
 * @note Requires OpenCL 1.2+ and GPU with X compute capability
 * @warning Falls back to CPU if GPU unavailable
 */
class MyKernel {
    // ...
};
```

## Review Checklist

Before committing GPU code:

- [ ] Code compiles with and without `-DENABLE_GPU=ON`
- [ ] All GPU classes have CPU fallback
- [ ] RAII used for all GPU resources
- [ ] Exception handling in all OpenCL calls
- [ ] Tests added and building (even if can't run without GPU)
- [ ] Documentation complete with examples
- [ ] CMake properly detects dependencies
- [ ] No hardcoded assumptions about GPU availability
- [ ] Memory cleanup verified (no leaks)
- [ ] Performance targets documented

## Quick Reference

**Build GPU-enabled:**
```bash
cmake .. -DENABLE_GPU=ON -DUSE_CLFFT=ON
```

**Run tests:**
```bash
ctest -V
```

**Check OpenCL availability:**
```bash
clinfo  # List OpenCL platforms/devices
```

**Common OpenCL errors:**
- `-2`: `CL_DEVICE_NOT_FOUND` - No GPU
- `-36`: `CL_INVALID_COMMAND_QUEUE` - Queue error
- `-46`: `CL_INVALID_KERNEL_NAME` - Kernel name mismatch
- `-48`: `CL_INVALID_PROGRAM` - Program build failed

## Summary

**Key Differences from Python GPU Work:**
- C++17 + OpenCL vs Python + CuPy
- RAII pattern vs context managers
- Compile-time conditional compilation vs runtime checks
- clFFT vs CuPy FFT
- Manual memory management vs automatic

**Three Rules:**
1. ✅ **RAII Always** - Automatic resource cleanup
2. 🔄 **CPU Fallback** - Every GPU path needs CPU equivalent
3. 🧪 **Test Compilation** - Even without GPU, code must compile

Following these guidelines ensures robust, portable GPU acceleration in the C++ prototype.
