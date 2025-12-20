# Phase 4.2-4.4: GPU Acceleration Architecture

**Document Version:** 1.0  
**Date:** December 2025  
**Status:** Architecture Complete - Implementation Roadmap Defined

---

## Executive Summary

This document provides complete technical specifications for implementing GPU acceleration in VHS-Decode C++ prototype (Phases 4.2-4.4). The architecture builds upon the completed Phase 4.1 OpenCL infrastructure to deliver 100-400x overall performance improvement.

**Current Performance (Phases 1-4.1):**
- 55 FPS (22x vs Python GPU baseline)
- CPU-based processing with FFTW3
- OpenCL infrastructure ready

**Target Performance (Phases 4.2-4.4):**
- 400-1000 FPS (160-400x vs Python GPU baseline)
- GPU-accelerated processing
- Hybrid CPU/GPU with automatic selection

**Implementation Timeline:** 7-10 weeks of focused development

---

## Table of Contents

1. [Phase 4.2: clFFT GPU FFT Integration](#phase-42-clfft-gpu-fft-integration)
2. [Phase 4.3: GPU Kernel Development](#phase-43-gpu-kernel-development)
3. [Phase 4.4: CPU/GPU Hybrid Processing](#phase-44-cpugpu-hybrid-processing)
4. [Performance Projections](#performance-projections)
5. [Implementation Roadmap](#implementation-roadmap)
6. [Cross-Platform Considerations](#cross-platform-considerations)
7. [Risk Assessment](#risk-assessment)

---

## Phase 4.2: clFFT GPU FFT Integration

### Overview

Replace CPU-based FFTW3 with GPU-accelerated clFFT library for massive speedup in FFT operations (currently 74% of processing time).

**Timeline:** 2-3 weeks  
**Expected Speedup:** 200x for FFT operations  
**Impact:** ~150x overall pipeline speedup

### Architecture

#### CLFFTEngine Class Design

```cpp
namespace gpu {

class CLFFTEngine {
public:
    // Constructor
    CLFFTEngine(OpenCLContext& ctx, size_t blockSize, bool useGPU = true);
    ~CLFFTEngine();
    
    // Deleted copy, move allowed
    CLFFTEngine(const CLFFTEngine&) = delete;
    CLFFTEngine& operator=(const CLFFTEngine&) = delete;
    CLFFTEngine(CLFFTEngine&&) noexcept;
    CLFFTEngine& operator=(CLFFTEngine&&) noexcept;
    
    // CPU-compatible interface (matches FFTWEngine)
    std::vector<std::complex<double>> forwardFFT(
        const std::vector<double>& input
    );
    
    std::vector<double> inverseFFT(
        const std::vector<std::complex<double>>& input
    );
    
    // GPU-optimized interface (avoids host transfers)
    void forwardFFTGPU(
        cl::Buffer& inputBuffer,      // real data (N samples)
        cl::Buffer& outputBuffer,     // complex data (N/2+1 bins)
        bool keepOnDevice = true
    );
    
    void inverseFFTGPU(
        cl::Buffer& inputBuffer,      // complex data (N/2+1 bins)
        cl::Buffer& outputBuffer,     // real data (N samples)
        bool keepOnDevice = true
    );
    
    // Filter application (on GPU, no transfer)
    void applyFilterGPU(
        cl::Buffer& fftData,          // complex FFT data (in/out)
        cl::Buffer& filter,           // real filter coefficients
        size_t filterSize
    );
    
    // Info
    size_t getBlockSize() const { return blockSize_; }
    bool isGPUEnabled() const { return useGPU_; }
    
private:
    OpenCLContext& context_;
    clfftPlanHandle forwardPlan_;
    clfftPlanHandle inversePlan_;
    size_t blockSize_;
    bool useGPU_;
    
    // Temporary buffers for GPU operations
    cl::Buffer tempInputBuffer_;
    cl::Buffer tempOutputBuffer_;
    
    void createPlans();
    void destroyPlans();
};

} // namespace gpu
```

#### CMake Integration

**FindclFFT.cmake:**

```cmake
# Finds clFFT library and headers
# Sets: clFFT_FOUND, clFFT_INCLUDE_DIRS, clFFT_LIBRARIES

find_path(clFFT_INCLUDE_DIR
    NAMES clFFT.h
    PATHS
        /usr/include
        /usr/local/include
        ${clFFT_ROOT}/include
        $ENV{clFFT_ROOT}/include
)

find_library(clFFT_LIBRARY
    NAMES clFFT
    PATHS
        /usr/lib
        /usr/local/lib
        ${clFFT_ROOT}/lib
        $ENV{clFFT_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(clFFT
    REQUIRED_VARS clFFT_LIBRARY clFFT_INCLUDE_DIR
)

if(clFFT_FOUND)
    set(clFFT_LIBRARIES ${clFFT_LIBRARY})
    set(clFFT_INCLUDE_DIRS ${clFFT_INCLUDE_DIR})
    
    add_library(clFFT::clFFT UNKNOWN IMPORTED)
    set_target_properties(clFFT::clFFT PROPERTIES
        IMPORTED_LOCATION "${clFFT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${clFFT_INCLUDE_DIR}"
    )
endif()
```

**Root CMakeLists.txt:**

```cmake
# After OpenCL detection
if(OpenCL_FOUND)
    # Find clFFT for GPU FFT acceleration
    find_package(clFFT)
    
    if(clFFT_FOUND)
        message(STATUS "clFFT found: ${clFFT_VERSION}")
        message(STATUS "  Include: ${clFFT_INCLUDE_DIRS}")
        message(STATUS "  Library: ${clFFT_LIBRARIES}")
        
        add_definitions(-DHAVE_CLFFT)
        include_directories(${clFFT_INCLUDE_DIRS})
        
        target_link_libraries(vhs-decode PRIVATE clFFT::clFFT)
    else()
        message(WARNING "clFFT not found - GPU FFT unavailable")
    endif()
endif()
```

#### Implementation Details

**Plan Creation:**

```cpp
void CLFFTEngine::createPlans() {
    clfftStatus status;
    
    // Create forward plan (real to complex)
    size_t lengths[] = { blockSize_ };
    status = clfftCreateDefaultPlan(&forwardPlan_, 
                                     context_.getContext()(),
                                     CLFFT_1D,
                                     lengths);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to create forward FFT plan");
    }
    
    status = clfftSetPlanPrecision(forwardPlan_, CLFFT_DOUBLE);
    status = clfftSetLayout(forwardPlan_, CLFFT_REAL, CLFFT_HERMITIAN_INTERLEAVED);
    status = clfftSetResultLocation(forwardPlan_, CLFFT_OUTOFPLACE);
    
    status = clfftBakePlan(forwardPlan_, 1, &context_.getQueue()(), nullptr, nullptr);
    
    // Create inverse plan (complex to real)
    status = clfftCreateDefaultPlan(&inversePlan_,
                                     context_.getContext()(),
                                     CLFFT_1D,
                                     lengths);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to create inverse FFT plan");
    }
    
    status = clfftSetPlanPrecision(inversePlan_, CLFFT_DOUBLE);
    status = clfftSetLayout(inversePlan_, CLFFT_HERMITIAN_INTERLEAVED, CLFFT_REAL);
    status = clfftSetResultLocation(inversePlan_, CLFFT_OUTOFPLACE);
    
    status = clfftBakePlan(inversePlan_, 1, &context_.getQueue()(), nullptr, nullptr);
}
```

**Forward FFT:**

```cpp
void CLFFTEngine::forwardFFTGPU(cl::Buffer& inputBuffer,
                                 cl::Buffer& outputBuffer,
                                 bool keepOnDevice) {
    clfftStatus status = clfftEnqueueTransform(
        forwardPlan_,
        CLFFT_FORWARD,
        1,
        &context_.getQueue()(),
        0,
        nullptr,
        nullptr,
        &inputBuffer(),
        &outputBuffer(),
        nullptr
    );
    
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Forward FFT failed: " + std::to_string(status));
    }
    
    if (!keepOnDevice) {
        context_.getQueue().finish();  // Synchronize
    }
}
```

#### Integration with RFProcessor

```cpp
class RFProcessor {
private:
    #ifdef HAVE_CLFFT
    std::unique_ptr<gpu::CLFFTEngine> gpuFFT_;
    #endif
    std::unique_ptr<rf::FFTWEngine> cpuFFT_;
    bool useGPU_;

public:
    RFProcessor(const ProcessingConfig& config) {
        #ifdef HAVE_CLFFT
        try {
            gpu::OpenCLContext ctx;
            gpuFFT_ = std::make_unique<gpu::CLFFTEngine>(ctx, config.blockSize);
            useGPU_ = true;
            std::cout << "Using GPU FFT (clFFT)" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "GPU FFT init failed: " << e.what() << std::endl;
            useGPU_ = false;
        }
        #else
        useGPU_ = false;
        #endif
        
        if (!useGPU_) {
            cpuFFT_ = std::make_unique<rf::FFTWEngine>(config.blockSize);
            std::cout << "Using CPU FFT (FFTW3)" << std::endl;
        }
    }
    
    AnalyticSignal processRF(const std::vector<uint8_t>& rfData) {
        if (useGPU_) {
            return processRFGPU(rfData);
        } else {
            return processRFCPU(rfData);
        }
    }
};
```

### Performance Analysis

**Current (FFTW3 CPU):**
- Forward FFT: 0.89 ms per 32K block
- Inverse FFT: 0.85 ms per 32K block
- Total FFT time: 1.74 ms (74% of pipeline)

**Target (clFFT GPU):**
- Forward FFT: ~0.004 ms
- Inverse FFT: ~0.005 ms
- Total FFT time: 0.009 ms

**Speedup:** 193x for FFT operations

### Dependencies

- clFFT library (https://github.com/clMathLibraries/clFFT)
- OpenCL 1.2+ compatible GPU
- Phase 4.1 OpenCL infrastructure

---

## Phase 4.3: GPU Kernel Development

### Overview

Develop custom OpenCL kernels for compute-intensive operations that benefit from massive parallelism.

**Timeline:** 3-4 weeks  
**Expected Speedup:** 100-1000x for element-wise operations  
**Impact:** Additional 10-50x overall pipeline speedup

### Kernel 1: Phase Unwrap

**Purpose:** Unwrap phase discontinuities in FM demodulated signal

**Current CPU Implementation:** 0.10 ms per block  
**Target GPU Performance:** 0.0001 ms per block  
**Expected Speedup:** 1000x

**OpenCL Kernel:**

```cpp
// phase_unwrap.cl
__kernel void phaseUnwrap(
    __global const float* phase,      // Input: raw phase
    __global float* unwrapped,        // Output: unwrapped phase
    const int N                        // Number of samples
) {
    int idx = get_global_id(0);
    
    if (idx >= N) return;
    
    if (idx == 0) {
        unwrapped[0] = phase[0];
        return;
    }
    
    // Calculate phase difference
    float delta = phase[idx] - phase[idx-1];
    
    // Unwrap: if |delta| > π, add/subtract 2π
    if (delta > M_PI_F) {
        delta -= 2.0f * M_PI_F;
    } else if (delta < -M_PI_F) {
        delta += 2.0f * M_PI_F;
    }
    
    // Accumulate
    unwrapped[idx] = unwrapped[idx-1] + delta;
}
```

**Host Code:**

```cpp
class PhaseUnwrapKernel {
public:
    PhaseUnwrapKernel(OpenCLContext& ctx) : context_(ctx) {
        // Load and compile kernel
        std::string source = loadKernelSource("phase_unwrap.cl");
        program_ = cl::Program(context_.getContext(), source);
        program_.build();
        kernel_ = cl::Kernel(program_, "phaseUnwrap");
    }
    
    void execute(cl::Buffer& phaseBuffer,
                 cl::Buffer& unwrappedBuffer,
                 size_t N) {
        // Set kernel arguments
        kernel_.setArg(0, phaseBuffer);
        kernel_.setArg(1, unwrappedBuffer);
        kernel_.setArg(2, static_cast<int>(N));
        
        // Execute
        size_t globalSize = N;
        size_t localSize = 256;  // Work group size
        
        context_.getQueue().enqueueNDRangeKernel(
            kernel_,
            cl::NullRange,
            cl::NDRange(globalSize),
            cl::NDRange(localSize)
        );
    }

private:
    OpenCLContext& context_;
    cl::Program program_;
    cl::Kernel kernel_;
};
```

### Kernel 2: Envelope Detection

**Purpose:** Calculate signal envelope from analytic signal (magnitude)

**Current CPU Implementation:** 0.15 ms per block  
**Target GPU Performance:** 0.0003 ms per block  
**Expected Speedup:** 500x

**OpenCL Kernel:**

```cpp
// envelope_detect.cl
__kernel void envelopeDetect(
    __global const float2* analyticSignal,  // Input: complex analytic signal
    __global float* envelope,                // Output: envelope (magnitude)
    const int N                              // Number of samples
) {
    int idx = get_global_id(0);
    
    if (idx >= N) return;
    
    float2 sample = analyticSignal[idx];
    
    // Compute magnitude: sqrt(real^2 + imag^2)
    float magnitude = sqrt(sample.x * sample.x + sample.y * sample.y);
    
    envelope[idx] = magnitude;
}
```

**Optimized Version (avoid sqrt):**

```cpp
// envelope_detect_fast.cl
__kernel void envelopeDetectFast(
    __global const float2* analyticSignal,
    __global float* envelope,
    const int N
) {
    int idx = get_global_id(0);
    if (idx >= N) return;
    
    float2 sample = analyticSignal[idx];
    
    // Fast approximation: max(|real|, |imag|) + 0.5 * min(|real|, |imag|)
    float absReal = fabs(sample.x);
    float absImag = fabs(sample.y);
    float maxVal = fmax(absReal, absImag);
    float minVal = fmin(absReal, absImag);
    
    envelope[idx] = maxVal + 0.5f * minVal;
}
```

### Kernel 3: Filter Application

**Purpose:** Apply frequency-domain filter to FFT data

**Current CPU Implementation:** 0.05 ms per block  
**Target GPU Performance:** 0.0001 ms per block  
**Expected Speedup:** 500x

**OpenCL Kernel:**

```cpp
// apply_filter.cl
__kernel void applyFilter(
    __global float2* fftData,          // Input/Output: complex FFT data
    __global const float* filter,      // Input: real filter coefficients
    const int N                        // Number of frequency bins
) {
    int idx = get_global_id(0);
    
    if (idx >= N) return;
    
    float filterCoeff = filter[idx];
    
    // Multiply complex number by real coefficient
    fftData[idx].x *= filterCoeff;  // Real part
    fftData[idx].y *= filterCoeff;  // Imaginary part
}
```

**Vectorized Version (4-wide):**

```cpp
// apply_filter_vec4.cl
__kernel void applyFilterVec4(
    __global float2* fftData,
    __global const float* filter,
    const int N
) {
    int idx = get_global_id(0) * 4;  // Process 4 elements per work item
    
    if (idx + 3 >= N) return;
    
    // Load 4 filter coefficients
    float4 filterCoeffs = vload4(idx / 4, filter);
    
    // Load 4 complex values (8 floats)
    float2 fft0 = fftData[idx];
    float2 fft1 = fftData[idx + 1];
    float2 fft2 = fftData[idx + 2];
    float2 fft3 = fftData[idx + 3];
    
    // Apply filter
    fft0 *= filterCoeffs.x;
    fft1 *= filterCoeffs.y;
    fft2 *= filterCoeffs.z;
    fft3 *= filterCoeffs.w;
    
    // Store results
    fftData[idx] = fft0;
    fftData[idx + 1] = fft1;
    fftData[idx + 2] = fft2;
    fftData[idx + 3] = fft3;
}
```

### Kernel Manager

**Unified Kernel Management:**

```cpp
class GPUKernelManager {
public:
    GPUKernelManager(OpenCLContext& ctx) : context_(ctx) {
        loadKernels();
    }
    
    void phaseUnwrap(cl::Buffer& phase, cl::Buffer& unwrapped, size_t N);
    void envelopeDetect(cl::Buffer& analytic, cl::Buffer& envelope, size_t N);
    void applyFilter(cl::Buffer& fftData, cl::Buffer& filter, size_t N);
    
private:
    OpenCLContext& context_;
    cl::Program program_;
    cl::Kernel phaseUnwrapKernel_;
    cl::Kernel envelopeDetectKernel_;
    cl::Kernel applyFilterKernel_;
    
    void loadKernels() {
        // Load all kernel sources
        std::string allKernels = 
            loadKernelSource("phase_unwrap.cl") + "\n" +
            loadKernelSource("envelope_detect.cl") + "\n" +
            loadKernelSource("apply_filter.cl");
        
        // Compile
        program_ = cl::Program(context_.getContext(), allKernels);
        program_.build();
        
        // Create kernel objects
        phaseUnwrapKernel_ = cl::Kernel(program_, "phaseUnwrap");
        envelopeDetectKernel_ = cl::Kernel(program_, "envelopeDetect");
        applyFilterKernel_ = cl::Kernel(program_, "applyFilter");
    }
};
```

---

## Phase 4.4: CPU/GPU Hybrid Processing

### Overview

Implement intelligent workload distribution between CPU and GPU with automatic selection, memory optimization, and fallback strategies.

**Timeline:** 2-3 weeks  
**Expected Benefit:** Optimal performance across hardware configurations  
**Impact:** Additional 2-5x from optimization

### Workload Analysis

**Profile Structure:**

```cpp
struct WorkloadProfile {
    size_t dataSize;              // Input data size (bytes)
    size_t fftOperations;         // Number of FFT operations
    size_t kernelOperations;      // Number of custom kernel calls
    size_t transferSize;          // Expected data transfer size
    
    double cpuTimeEstimate;       // Estimated CPU time (ms)
    double gpuTimeEstimate;       // Estimated GPU time (ms)
    double transferOverhead;      // Transfer overhead (ms)
    
    bool useGPU() const {
        return (gpuTimeEstimate + transferOverhead) < cpuTimeEstimate;
    }
};
```

**Profiler:**

```cpp
class WorkloadProfiler {
public:
    WorkloadProfile analyzeBlock(const RFBlock& block) {
        WorkloadProfile profile;
        
        profile.dataSize = block.data.size();
        profile.fftOperations = 2;  // Forward + inverse
        profile.kernelOperations = 3;  // Unwrap + envelope + filter
        profile.transferSize = block.data.size() * sizeof(float);
        
        // Estimate CPU time (from benchmarks)
        profile.cpuTimeEstimate = 
            profile.fftOperations * CPU_FFT_TIME_PER_OP +
            profile.kernelOperations * CPU_KERNEL_TIME_PER_OP;
        
        // Estimate GPU time
        profile.gpuTimeEstimate =
            profile.fftOperations * GPU_FFT_TIME_PER_OP +
            profile.kernelOperations * GPU_KERNEL_TIME_PER_OP;
        
        // Transfer overhead (PCIe bandwidth)
        profile.transferOverhead =
            (profile.transferSize * 2) / PCIE_BANDWIDTH;  // Upload + download
        
        return profile;
    }
    
private:
    static constexpr double CPU_FFT_TIME_PER_OP = 0.87;  // ms
    static constexpr double CPU_KERNEL_TIME_PER_OP = 0.08;
    static constexpr double GPU_FFT_TIME_PER_OP = 0.004;
    static constexpr double GPU_KERNEL_TIME_PER_OP = 0.0002;
    static constexpr double PCIE_BANDWIDTH = 28000.0;  // MB/s (PCIe 4.0)
};
```

### Processor Selection

**Selection Strategy:**

```cpp
enum class ProcessingMode {
    CPU,
    GPU,
    HYBRID
};

class ProcessorSelector {
public:
    ProcessingMode selectProcessor(const WorkloadProfile& profile) {
        // GPU unavailable → CPU
        if (!gpuAvailable_) {
            return ProcessingMode::CPU;
        }
        
        // Small data → CPU (transfer overhead dominates)
        if (profile.dataSize < SMALL_DATA_THRESHOLD) {
            return ProcessingMode::CPU;
        }
        
        // Large data with many operations → GPU
        if (profile.dataSize > LARGE_DATA_THRESHOLD &&
            profile.fftOperations > 2) {
            return ProcessingMode::GPU;
        }
        
        // Estimate and choose best
        bool gpuFaster = profile.useGPU();
        return gpuFaster ? ProcessingMode::GPU : ProcessingMode::CPU;
    }
    
private:
    bool gpuAvailable_;
    static constexpr size_t SMALL_DATA_THRESHOLD = 4096;
    static constexpr size_t LARGE_DATA_THRESHOLD = 16384;
};
```

### Memory Transfer Optimization

**Pinned Memory:**

```cpp
class PinnedMemoryManager {
public:
    PinnedMemoryManager(OpenCLContext& ctx, size_t maxSize) 
        : context_(ctx), maxSize_(maxSize) {
        
        // Allocate pinned host memory
        pinnedBuffer_ = context_.getContext().createBuffer(
            CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
            maxSize_
        );
        
        pinnedPtr_ = context_.getQueue().enqueueMapBuffer(
            pinnedBuffer_,
            CL_TRUE,
            CL_MAP_READ | CL_MAP_WRITE,
            0,
            maxSize_
        );
    }
    
    ~PinnedMemoryManager() {
        context_.getQueue().enqueueUnmapMemObject(pinnedBuffer_, pinnedPtr_);
    }
    
    void* getPinnedPointer() { return pinnedPtr_; }
    cl::Buffer& getBuffer() { return pinnedBuffer_; }
    
private:
    OpenCLContext& context_;
    size_t maxSize_;
    cl::Buffer pinnedBuffer_;
    void* pinnedPtr_;
};
```

**Asynchronous Transfers:**

```cpp
class AsyncTransferManager {
public:
    AsyncTransferManager(OpenCLContext& ctx) : context_(ctx) {
        // Create separate queues for transfer and compute
        transferQueue_ = cl::CommandQueue(
            context_.getContext(),
            context_.getDevice(),
            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
        );
        
        computeQueue_ = cl::CommandQueue(
            context_.getContext(),
            context_.getDevice(),
            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
        );
    }
    
    // Overlap transfer of block N+1 with processing of block N
    void pipelinedProcess(const std::vector<RFBlock>& blocks) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            // Upload block i+1 (if exists) while processing block i
            cl::Event uploadEvent, computeEvent, downloadEvent;
            
            if (i + 1 < blocks.size()) {
                uploadAsync(blocks[i+1], uploadEvent);
            }
            
            processGPU(blocks[i], computeEvent);
            downloadAsync(blocks[i], downloadEvent);
            
            // Wait for download to complete before using results
            downloadEvent.wait();
        }
    }
    
private:
    OpenCLContext& context_;
    cl::CommandQueue transferQueue_;
    cl::CommandQueue computeQueue_;
};
```

### Hybrid Processing Pipeline

**Complete Implementation:**

```cpp
class HybridProcessor {
public:
    HybridProcessor(const ProcessingConfig& config)
        : config_(config),
          profiler_(),
          selector_() {
        
        // Initialize CPU processor
        cpuProcessor_ = std::make_unique<CPUProcessor>(config);
        
        // Try to initialize GPU processor
        try {
            context_ = std::make_unique<OpenCLContext>();
            gpuProcessor_ = std::make_unique<GPUProcessor>(*context_, config);
            gpuAvailable_ = true;
        } catch (const std::exception& e) {
            std::cerr << "GPU init failed: " << e.what() << std::endl;
            gpuAvailable_ = false;
        }
    }
    
    ProcessingResult process(const RFBlock& block) {
        // Profile workload
        auto profile = profiler_.analyzeBlock(block);
        
        // Select processor
        auto mode = selector_.selectProcessor(profile);
        
        // Process
        try {
            if (mode == ProcessingMode::GPU && gpuAvailable_) {
                return gpuProcessor_->process(block);
            } else {
                return cpuProcessor_->process(block);
            }
        } catch (const cl::Error& e) {
            // GPU error → fall back to CPU
            std::cerr << "GPU error: " << e.what() 
                      << ", falling back to CPU" << std::endl;
            return cpuProcessor_->process(block);
        }
    }
    
private:
    ProcessingConfig config_;
    WorkloadProfiler profiler_;
    ProcessorSelector selector_;
    
    std::unique_ptr<CPUProcessor> cpuProcessor_;
    std::unique_ptr<OpenCLContext> context_;
    std::unique_ptr<GPUProcessor> gpuProcessor_;
    bool gpuAvailable_ = false;
};
```

---

## Performance Projections

### Component-Level Analysis

| Component | CPU Time | GPU Time | Speedup | % of Pipeline |
|-----------|----------|----------|---------|---------------|
| **FFT Forward** | 0.89 ms | 0.004 ms | 222x | 38% |
| **FFT Inverse** | 0.85 ms | 0.005 ms | 170x | 36% |
| **Phase Unwrap** | 0.10 ms | 0.0001 ms | 1000x | 4% |
| **Envelope Detect** | 0.15 ms | 0.0003 ms | 500x | 6% |
| **Filter Application** | 0.05 ms | 0.0001 ms | 500x | 2% |
| **FM Demodulation** | 0.10 ms | 0.0005 ms | 200x | 4% |
| **Video/Chroma Sep** | 0.20 ms | 0.001 ms | 200x | 9% |
| **I/O & Misc** | 0.03 ms | 0.03 ms | 1x | 1% |
| **TOTAL** | **2.37 ms** | **0.0105 ms** | **226x** | **100%** |

### Overall Pipeline Performance

**Current (Phase 3 CPU):**
- Per-block time: 2.34 ms (single-thread)
- Throughput: 427 blocks/second
- Frame rate: ~17 FPS (single-thread)
- With 4 threads: ~55 FPS

**Target (Phase 4 GPU):**
- Per-block time: 0.01 ms (single GPU stream)
- Throughput: 100,000 blocks/second
- Frame rate: ~400 FPS (single stream)
- With batching/multi-stream: 1000+ FPS

**Real-World Considerations:**
- Memory transfer overhead: +20-40% (→ 0.014 ms)
- GPU initialization: one-time cost (~100 ms)
- Multi-stream concurrency: 2-3x additional throughput
- Practical sustained rate: 400-1000 FPS

### Speedup Summary

| Version | FPS | vs Python GPU | vs C++ CPU |
|---------|-----|---------------|------------|
| Python GPU | 2.49 | 1.0x | - |
| Python CPU (8-thread) | 2.19 | 0.88x | - |
| C++ CPU (4-thread) | 55.0 | 22.1x | 1.0x |
| **C++ GPU (Phase 4)** | **400-1000** | **160-400x** | **7-18x** |

---

## Implementation Roadmap

### Phase 4.2: clFFT Integration (Weeks 1-3)

**Week 1: Setup & Foundation**
- [ ] Install clFFT library and headers
- [ ] Create CMake FindclFFT.cmake module
- [ ] Test clFFT basic functionality
- [ ] Create CLFFTEngine class skeleton

**Week 2: Implementation**
- [ ] Implement plan creation/destruction
- [ ] Implement forward FFT (real → complex)
- [ ] Implement inverse FFT (complex → real)
- [ ] Add filter application on GPU
- [ ] Error handling and fallback

**Week 3: Integration & Testing**
- [ ] Integrate CLFFTEngine with RFProcessor
- [ ] Benchmark vs FFTW3
- [ ] Validate numerical accuracy
- [ ] Cross-platform testing
- [ ] Documentation

**Deliverables:**
- Working CLFFTEngine class
- Integrated with RFProcessor
- 200x FFT speedup validated
- CPU fallback tested

### Phase 4.3: GPU Kernels (Weeks 4-7)

**Week 4: Phase Unwrap Kernel**
- [ ] Write phase_unwrap.cl kernel
- [ ] Create PhaseUnwrapKernel host class
- [ ] Test and validate correctness
- [ ] Optimize work group size
- [ ] Benchmark vs CPU

**Week 5: Envelope & Filter Kernels**
- [ ] Write envelope_detect.cl kernel
- [ ] Write apply_filter.cl kernel
- [ ] Create host wrapper classes
- [ ] Test and validate correctness
- [ ] Optimize performance

**Week 6: Kernel Manager & Integration**
- [ ] Create GPUKernelManager class
- [ ] Integrate kernels into pipeline
- [ ] Add error handling
- [ ] Test complete GPU path
- [ ] Benchmark end-to-end

**Week 7: Optimization & Multi-GPU**
- [ ] Profile kernel performance
- [ ] Optimize memory access patterns
- [ ] Add multi-GPU support
- [ ] Load balancing across GPUs
- [ ] Documentation

**Deliverables:**
- 3 optimized GPU kernels
- GPUKernelManager class
- Integrated into pipeline
- 100-1000x speedups validated

### Phase 4.4: Hybrid Processing (Weeks 8-10)

**Week 8: Workload Analysis**
- [ ] Create WorkloadProfiler class
- [ ] Implement estimation algorithms
- [ ] Create ProcessorSelector class
- [ ] Test selection logic
- [ ] Tune thresholds

**Week 9: Memory Optimization**
- [ ] Implement PinnedMemoryManager
- [ ] Create AsyncTransferManager
- [ ] Add pipelined processing
- [ ] Test transfer overlap
- [ ] Benchmark transfer rates

**Week 10: Integration & Final Testing**
- [ ] Create HybridProcessor class
- [ ] Integrate all components
- [ ] Add comprehensive error handling
- [ ] Cross-platform testing
- [ ] Performance validation
- [ ] Documentation

**Deliverables:**
- Complete hybrid processing system
- Automatic CPU/GPU selection
- Optimized memory transfers
- Comprehensive testing
- Final documentation

### Testing Strategy

**Unit Tests:**
- Each kernel tested independently
- CPU vs GPU result comparison
- Numerical accuracy validation
- Edge case handling

**Integration Tests:**
- Full pipeline GPU processing
- CPU/GPU hybrid switching
- Error recovery and fallback
- Multi-block processing

**Performance Tests:**
- Benchmark each component
- End-to-end pipeline benchmarks
- Different GPU vendors
- Various workload sizes

**Platform Tests:**
- Linux (NVIDIA/AMD/Intel GPUs)
- Windows (NVIDIA/AMD GPUs)
- macOS (if OpenCL available)

---

## Cross-Platform Considerations

### GPU Vendor Differences

**NVIDIA:**
- Best OpenCL support
- Excellent clFFT performance
- Large compute units
- Expected performance: 100% of target

**AMD:**
- Good OpenCL support
- clFFT optimized for AMD
- Different architecture considerations
- Expected performance: 80-90% of NVIDIA

**Intel:**
- OpenCL support via drivers
- Integrated GPU lower performance
- Good for development/testing
- Expected performance: 30-50% of NVIDIA

### Windows-Specific Considerations

**FFTW3 on Windows:**
- Use precompiled DLLs from fftw.org
- Set FFTW3_ROOT environment variable
- Link against libfftw3-3.dll

**OpenCL on Windows:**
- Included with GPU drivers
- NVIDIA: CUDA toolkit includes OpenCL
- AMD: Radeon drivers include OpenCL
- Intel: Graphics drivers include OpenCL

**clFFT on Windows:**
- Build from source or use vcpkg
- `vcpkg install clfft`
- Set clFFT_ROOT environment variable

**CMake Configuration (Windows):**

```powershell
cmake .. -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release `
  -DUSE_FFTW3=ON `
  -DUSE_OPENCL=ON `
  -DUSE_CLFFT=ON `
  -DFFTW3_ROOT="C:\fftw3" `
  -DOpenCL_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0" `
  -DclFFT_ROOT="C:\vcpkg\installed\x64-windows"
```

---

## Risk Assessment

### Technical Risks

**High Priority:**

1. **GPU Driver Compatibility**
   - **Risk:** Different vendors, driver versions
   - **Mitigation:** Extensive testing, graceful fallbacks
   - **Impact:** Medium-High

2. **Memory Transfer Overhead**
   - **Risk:** PCIe bandwidth limitations
   - **Mitigation:** Pinned memory, asynchronous transfers, batching
   - **Impact:** Medium

3. **Numerical Precision**
   - **Risk:** GPU float precision differences
   - **Mitigation:** Use double precision, validate against CPU
   - **Impact:** Low-Medium

**Medium Priority:**

4. **clFFT Library Issues**
   - **Risk:** Build issues, platform support
   - **Mitigation:** CPU FFT fallback, clear documentation
   - **Impact:** Low-Medium

5. **Multi-GPU Support**
   - **Risk:** Complexity in load balancing
   - **Mitigation:** Start with single GPU, add multi-GPU later
   - **Impact:** Low

**Low Priority:**

6. **macOS OpenCL Deprecation**
   - **Risk:** Apple deprecated OpenCL in favor of Metal
   - **Mitigation:** Still functional, consider Metal port later
   - **Impact:** Low

### Mitigation Strategies

**For All Risks:**
- Comprehensive error handling
- Automatic fallback to CPU
- Extensive testing across platforms
- Clear user documentation
- Diagnostic tools for troubleshooting

**Specific Mitigations:**
- Driver issues → Test on multiple GPU models
- Transfer overhead → Optimize with pinning and async
- Precision → Use double, validate numerics
- Build issues → Clear build instructions, Docker containers
- Multi-GPU → Phase 2 feature, not MVP

---

## Success Criteria

### Performance Targets

- ✅ FFT operations: 200x speedup
- ✅ Phase unwrap: 1000x speedup
- ✅ Envelope detection: 500x speedup
- ✅ Overall pipeline: 100-400x speedup
- ✅ Sustained frame rate: 400-1000 FPS

### Quality Targets

- ✅ Numerical accuracy: < 1e-9 error vs CPU
- ✅ Cross-platform: Linux, Windows (macOS if possible)
- ✅ GPU vendors: NVIDIA, AMD, Intel
- ✅ Error recovery: Automatic CPU fallback
- ✅ Code quality: Modern C++17, RAII, exception-safe

### Documentation Targets

- ✅ Implementation guide
- ✅ API documentation
- ✅ Build instructions (all platforms)
- ✅ Performance tuning guide
- ✅ Troubleshooting guide

---

## Conclusion

Phase 4.2-4.4 GPU acceleration architecture provides a complete roadmap for achieving 100-400x overall performance improvement. The design builds upon the solid foundation of Phase 4.1 OpenCL infrastructure and maintains compatibility with the existing CPU pipeline.

**Key Strengths:**
- Modular design with clear separation of concerns
- Automatic CPU/GPU selection for optimal performance
- Comprehensive error handling and fallback mechanisms
- Cross-platform support (Linux/Windows/macOS)
- Multi-vendor GPU support (NVIDIA/AMD/Intel)

**Implementation Path:**
- Phase 4.2 (clFFT): 2-3 weeks → 200x FFT speedup
- Phase 4.3 (Kernels): 3-4 weeks → 100-1000x kernel speedups
- Phase 4.4 (Hybrid): 2-3 weeks → Optimal performance
- **Total: 7-10 weeks** to full GPU acceleration

**Recommendation:** Proceed with implementation following this architecture to achieve target 400-1000 FPS performance (160-400x vs Python baseline).

---

**Document Status:** Complete and ready for implementation  
**Next Step:** Begin Phase 4.2 clFFT integration

