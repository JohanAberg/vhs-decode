# VHS-Decode C++/OpenCL Rewrite Plan

**Version**: 1.0  
**Date**: December 19, 2025  
**Status**: Architectural Design & Implementation Roadmap  
**Estimated Timeline**: 12-18 months  
**Expected Performance Gain**: 5-20x speedup over current GPU implementation  

---

## Executive Summary

This document outlines a comprehensive plan to rewrite VHS-Decode from Python/NumPy/CuPy to native C++17/20 with OpenCL for GPU acceleration. The current codebase consists of ~36,000 lines of Python code with some Cython optimizations and experimental CuPy GPU support. A native rewrite will deliver:

- **5-20x performance improvement** over current GPU implementation (2.49 FPS → 12-50 FPS)
- **Cross-platform GPU support** via OpenCL (NVIDIA, AMD, Intel)
- **Lower memory footprint** through efficient C++ memory management
- **Better multi-threading** with native thread pools and SIMD
- **Easier deployment** as standalone executables without Python runtime
- **Professional code quality** with modern C++ practices

### Current State Analysis

**Python Codebase (~36,000 lines):**
- Core decoder: `lddecode/core.py` (4,514 lines)
- VHS processor: `vhsdecode/process.py` (1,325 lines) + `process_gpu.py` (1,191 lines)
- Multiple format support: VHS, SVHS, Betamax, Video8, U-Matic, Type C, etc.
- Current GPU implementation uses CuPy (CUDA-only)
- Performance: 2.49 FPS (GPU) vs 2.19 FPS (8-thread CPU)

**Existing C++ Infrastructure:**
- CMake build system already in place
- Qt5/Qt6-based tools (ld-analyse, ld-process-vbi, etc.)
- TBC library in C++ (`tools/library/tbc/`)
- Signal processing library infrastructure

---

## Architecture Overview

### Current Python Architecture

```
decode.py (CLI Entry)
    ↓
VHSDecode (Main Decoder Class)
    ↓
VHSRFDecode / VHSRFDecodeGPU (RF Processing)
    ↓
DemodCache (Multi-threaded Block Processing)
    ↓
Block Processing Pipeline:
    1. FFT → RF Filtering → Hilbert Transform
    2. FM Demodulation → Phase Unwrapping
    3. Video/Chroma Separation
    4. Envelope Detection & Dropout Compensation
    5. Time-Base Correction
    ↓
Field Assembly & TBC Output
```

### Proposed C++ Architecture

```
vhs-decode-cpp (CLI Entry)
    ↓
VHSDecoder (Main Controller)
    ↓
├── RFProcessor (RF Signal Processing)
│   ├── FFTEngine (OpenCL-accelerated)
│   ├── FilterBank (Frequency-domain filters)
│   └── HilbertTransform
│
├── FMDemodulator (FM Demodulation)
│   ├── PhaseUnwrapper (OpenCL kernel)
│   ├── EnvelopeDetector
│   └── SpikeReplacer
│
├── VideoProcessor (Video Signal Processing)
│   ├── ChromaSeparator
│   ├── DropoutDetector
│   └── TimeBaseCorrector
│
├── GPUContext (OpenCL Management)
│   ├── DeviceManager
│   ├── KernelCompiler
│   ├── MemoryPool
│   └── CommandQueue
│
└── OutputWriter (TBC File I/O)
    ├── TBCWriter
    ├── JSONMetadata
    └── ChromaWriter
```

---

## Technology Stack

### Core C++ Technologies

**Language**: C++17 (minimum), C++20 (target)
- Modern C++ features: smart pointers, move semantics, structured bindings
- Ranges library (C++20) for cleaner data pipelines
- Concepts (C++20) for template constraints
- Coroutines (C++20) for async I/O

**Build System**: CMake 3.16+
- Existing CMake infrastructure can be extended
- Support for cross-platform builds (Linux, Windows, macOS)
- Automatic dependency management via vcpkg or Conan

**Threading**: C++ Standard Library + Intel TBB (optional)
- `std::thread` and `std::async` for basic parallelism
- Intel TBB for advanced task scheduling (optional dependency)
- Thread pool for block processing
- Lock-free queues for producer-consumer patterns

### GPU Acceleration

**Primary**: OpenCL 1.2+ (for maximum compatibility)
- Cross-vendor support (NVIDIA, AMD, Intel, Apple)
- Mature ecosystem with good tooling
- Direct control over kernels and memory

**Alternative/Future**: Vulkan Compute (optional)
- Better performance on modern GPUs
- More complex but future-proof
- Can be added as optional backend later

**CPU Fallback**: Always maintained
- Automatic fallback when GPU unavailable
- SIMD optimization via intrinsics or libraries
- Same code path for validation

### Signal Processing Libraries

**FFT Library**: 
- **clFFT** (OpenCL FFT library) for GPU
- **FFTW3** for CPU fallback (industry standard)
- **Intel MKL** (optional) for Intel CPUs
- **Pocket FFT** as lightweight alternative

**Linear Algebra**:
- **Eigen3** (header-only, fast, well-tested)
- Template-based, compile-time optimization
- SIMD support built-in
- Easy integration with OpenCL buffers

**DSP Primitives**:
- Custom implementations for:
  - Hilbert transforms
  - IIR/FIR filtering
  - Bandpass filters
  - Envelope detection
  - Phase unwrapping

### I/O and Utilities

**File I/O**:
- `std::filesystem` (C++17) for path handling
- Memory-mapped I/O for large RF captures
- Async I/O with `io_uring` (Linux) or IOCP (Windows)

**JSON**: 
- **nlohmann/json** (header-only, easy to use)
- For TBC metadata files

**Logging**:
- **spdlog** (fast, header-only option available)
- Structured logging with severity levels

**Command-line Parsing**:
- **CLI11** (modern, easy to use)
- Or **cxxopts** (lightweight alternative)

**Testing**:
- **Catch2** (modern, expressive)
- Unit tests for all components
- Integration tests with reference data

---

## Detailed Component Design

### 1. RF Processor

**Responsibilities**:
- Read raw RF samples (uint8, int16, float32)
- Block-based processing with overlap-save
- FFT-based filtering in frequency domain
- Hilbert transform for analytic signal

**Key Classes**:

```cpp
class RFProcessor {
public:
    struct Config {
        size_t blockSize;      // 32K - 131K samples
        size_t blockCut;       // Samples to discard at edges
        double inputFreq;      // 40 MSPS typical
        std::string format;    // VHS, SVHS, Betamax, etc.
    };

    RFProcessor(const Config& config, GPUContext* gpu);
    
    // Process one block of RF data
    ProcessedBlock processBlock(const RawBlock& input);
    
    // Batch processing for GPU efficiency
    std::vector<ProcessedBlock> processBlocks(
        const std::vector<RawBlock>& inputs
    );

private:
    std::unique_ptr<FFTEngine> fftEngine_;
    std::unique_ptr<FilterBank> filters_;
    std::unique_ptr<HilbertTransform> hilbert_;
    GPUContext* gpu_;
};

class FFTEngine {
public:
    // OpenCL-accelerated FFT
    FFTEngine(GPUContext* gpu, size_t blockSize);
    
    // Forward/inverse transforms
    ComplexArray forward(const RealArray& input);
    RealArray inverse(const ComplexArray& input);
    
    // In-place transforms (more efficient)
    void forwardInPlace(ComplexArray& data);
    void inverseInPlace(ComplexArray& data);
    
private:
    cl::Context context_;
    cl::CommandQueue queue_;
    clfftPlanHandle fftPlan_;
    size_t blockSize_;
};

class FilterBank {
public:
    // Pre-computed frequency-domain filters
    void loadFilters(const std::string& csvPath);
    
    // Apply filter in frequency domain
    void applyFilter(ComplexArray& fftData, 
                     const std::string& filterName);
    
    // Batch apply multiple filters
    std::map<std::string, ComplexArray> applyFilters(
        const ComplexArray& fftData,
        const std::vector<std::string>& filterNames
    );
    
private:
    std::map<std::string, ComplexArray> filters_;
    GPUContext* gpu_;
};
```

### 2. FM Demodulator

**Responsibilities**:
- Phase extraction from analytic signal
- Phase unwrapping (critical performance path)
- Envelope detection for dropout compensation
- Spike detection and replacement

**Key Classes**:

```cpp
class FMDemodulator {
public:
    struct DemodResult {
        RealArray video;       // Main video signal
        RealArray video05;     // 0.5 MHz filtered video
        RealArray chroma;      // Chroma carrier
        RealArray envelope;    // Envelope for DOD
    };

    FMDemodulator(const Config& config, GPUContext* gpu);
    
    DemodResult demodulate(const ComplexArray& analyticSignal);
    
private:
    std::unique_ptr<PhaseUnwrapper> unwrapper_;
    std::unique_ptr<EnvelopeDetector> envelopeDetector_;
    std::unique_ptr<SpikeReplacer> spikeReplacer_;
    GPUContext* gpu_;
};

class PhaseUnwrapper {
public:
    // OpenCL kernel for parallel phase unwrapping
    RealArray unwrap(const ComplexArray& analyticSignal);
    
    // Optimized algorithm:
    // 1. Compute phase differences (parallel)
    // 2. Detect wraps (parallel)
    // 3. Cumulative sum (parallel scan)
    
private:
    cl::Kernel unwrapKernel_;
    cl::Kernel scanKernel_;  // Parallel prefix sum
    GPUContext* gpu_;
};

class EnvelopeDetector {
public:
    // Envelope = abs(analytic_signal)
    RealArray detect(const ComplexArray& analyticSignal);
    
    // Apply IIR envelope filter
    RealArray filterEnvelope(const RealArray& envelope);
    
private:
    // IIR filter coefficients
    std::vector<double> sosCoeffs_;
    cl::Kernel absKernel_;
    GPUContext* gpu_;
};
```

### 3. Video Processor

**Responsibilities**:
- Chroma separation (color-under heterodyning)
- Time-base correction (TBC)
- Dropout detection and flagging
- Line sync detection

**Key Classes**:

```cpp
class VideoProcessor {
public:
    struct ProcessedField {
        std::vector<uint16_t> videoData;
        std::vector<uint16_t> chromaData;
        FieldMetadata metadata;
        std::vector<Dropout> dropouts;
    };

    VideoProcessor(const Config& config, GPUContext* gpu);
    
    ProcessedField processField(const DemodResult& demod);
    
private:
    std::unique_ptr<ChromaSeparator> chromaSep_;
    std::unique_ptr<DropoutDetector> dropoutDetect_;
    std::unique_ptr<TimeBaseCorrector> tbc_;
    std::unique_ptr<SyncDetector> syncDetect_;
    GPUContext* gpu_;
};

class ChromaSeparator {
public:
    // Heterodyne chroma signal for color-under formats
    ChromaResult separate(const RealArray& chroma, 
                          const std::string& format);
    
    // Supported formats: VHS, SVHS, Betamax, Video8, etc.
    
private:
    cl::Kernel heterodyneKernel_;
    GPUContext* gpu_;
};

class TimeBaseCorrector {
public:
    // Correct timing errors in video signal
    CorrectedField correct(const RawField& field,
                          const std::vector<SyncPulse>& syncs);
    
    // Adaptive sync tracking
    void updateTracking(const FieldMetadata& metadata);
    
private:
    double trackingPhase_;
    double trackingFreq_;
};
```

### 4. GPU Context Manager

**Responsibilities**:
- OpenCL platform/device selection
- Context and command queue management
- Kernel compilation and caching
- Memory pool management
- CPU fallback coordination

**Key Classes**:

```cpp
class GPUContext {
public:
    struct Config {
        bool enableGPU = true;
        int deviceID = 0;
        size_t maxMemoryMB = 0;  // 0 = auto-detect
        bool enableProfiling = false;
    };

    GPUContext(const Config& config);
    
    // Device info
    bool isGPUAvailable() const;
    std::string getDeviceName() const;
    size_t getAvailableMemory() const;
    
    // OpenCL access
    cl::Context& getContext();
    cl::CommandQueue& getQueue();
    cl::Device& getDevice();
    
    // Kernel management
    cl::Kernel compileKernel(const std::string& source,
                            const std::string& kernelName);
    cl::Kernel getKernel(const std::string& name);
    
    // Memory management
    template<typename T>
    cl::Buffer allocate(size_t count, cl_mem_flags flags);
    
    void freeUnused();  // Free unused buffers
    
    // Profiling
    void enableProfiling(bool enable);
    ProfilingStats getStats() const;
    
private:
    cl::Platform platform_;
    cl::Device device_;
    cl::Context context_;
    cl::CommandQueue queue_;
    
    std::map<std::string, cl::Kernel> kernelCache_;
    MemoryPool memoryPool_;
    ProfilingData profiling_;
};

class MemoryPool {
public:
    // Efficient buffer reuse
    cl::Buffer acquire(size_t size, cl_mem_flags flags);
    void release(cl::Buffer buffer);
    
    // Memory statistics
    size_t getTotalAllocated() const;
    size_t getPeakUsage() const;
    
private:
    std::map<size_t, std::vector<cl::Buffer>> freeBuffers_;
    std::set<cl::Buffer> activeBuffers_;
    size_t peakUsage_ = 0;
};
```

### 5. Output Writer

**Responsibilities**:
- Write TBC format (16-bit packed video)
- Write JSON metadata
- Optional chroma output
- Progress reporting

**Key Classes**:

```cpp
class TBCWriter {
public:
    TBCWriter(const std::string& basePath);
    
    void writeField(const ProcessedField& field);
    void writeMetadata(const TBCMetadata& metadata);
    void close();
    
    // Progress callback
    using ProgressCallback = std::function<void(size_t, size_t)>;
    void setProgressCallback(ProgressCallback callback);
    
private:
    std::ofstream videoFile_;
    std::ofstream chromaFile_;  // Optional
    std::unique_ptr<JSONWriter> jsonWriter_;
    size_t fieldsWritten_ = 0;
};

class JSONWriter {
public:
    // Build JSON metadata incrementally
    void addField(const FieldMetadata& metadata);
    void addDropout(size_t fieldNum, const Dropout& dropout);
    
    // Write to file
    void write(const std::string& path);
    
private:
    nlohmann::json metadata_;
};
```

---

## OpenCL Kernel Design

### Critical Kernels

**1. FFT (via clFFT library)**
- Use optimized clFFT for all FFT operations
- Pre-plan transforms at initialization
- Batch transforms when possible

**2. Phase Unwrapping Kernel**

```cpp
// phase_unwrap.cl
__kernel void unwrap_phase(
    __global const float2* analytic,  // Complex input
    __global float* unwrapped,        // Output phase
    __global float* work,             // Workspace for scan
    int n)
{
    int gid = get_global_id(0);
    if (gid >= n) return;
    
    // Compute phase angle
    float2 z = analytic[gid];
    float phase = atan2(z.y, z.x);
    
    // Compute phase difference
    float diff = 0.0f;
    if (gid > 0) {
        float prev = atan2(analytic[gid-1].y, analytic[gid-1].x);
        diff = phase - prev;
        
        // Wrap to [-π, π]
        while (diff > M_PI_F) diff -= 2.0f * M_PI_F;
        while (diff < -M_PI_F) diff += 2.0f * M_PI_F;
    }
    
    work[gid] = diff;
    barrier(CLK_GLOBAL_MEM_FENCE);
    
    // Parallel prefix sum (separate kernel for efficiency)
}

__kernel void prefix_sum(
    __global float* data,
    __global float* result,
    int n)
{
    // Efficient parallel scan algorithm
    // (Blelloch scan or work-efficient implementation)
    // ...
}
```

**3. Envelope Detection Kernel**

```cpp
// envelope.cl
__kernel void compute_envelope(
    __global const float2* analytic,  // Complex input
    __global float* envelope,         // Output envelope
    int n)
{
    int gid = get_global_id(0);
    if (gid >= n) return;
    
    float2 z = analytic[gid];
    envelope[gid] = sqrt(z.x * z.x + z.y * z.y);
}

__kernel void iir_filter_forward(
    __global const float* input,
    __global float* output,
    __global float* state,
    __constant float* coeffs,  // SOS coefficients
    int n,
    int sections)
{
    // Parallel IIR filtering using transposed Direct Form II
    // Process each biquad section in sequence but parallelize samples
    // ...
}
```

**4. Chroma Heterodyne Kernel**

```cpp
// chroma.cl
__kernel void heterodyne_chroma(
    __global const float* chroma_in,
    __global float* chroma_out,
    float carrier_freq,
    float target_freq,
    float sample_rate,
    int n)
{
    int gid = get_global_id(0);
    if (gid >= n) return;
    
    float t = (float)gid / sample_rate;
    float shift = 2.0f * M_PI_F * (target_freq - carrier_freq) * t;
    
    // Complex multiply for frequency shift
    float cos_shift = cos(shift);
    float sin_shift = sin(shift);
    
    chroma_out[gid] = chroma_in[gid] * cos_shift;
}
```

**5. Spike Detection & Replacement**

```cpp
// spike.cl
__kernel void detect_spikes(
    __global const float* signal,
    __global uchar* spike_mask,
    float threshold,
    int n)
{
    int gid = get_global_id(0);
    if (gid >= 2 && gid < n - 2) {
        float val = signal[gid];
        float avg = (signal[gid-2] + signal[gid-1] + 
                     signal[gid+1] + signal[gid+2]) * 0.25f;
        
        spike_mask[gid] = (fabs(val - avg) > threshold) ? 1 : 0;
    }
}

__kernel void replace_spikes(
    __global float* signal,
    __global const uchar* spike_mask,
    int n)
{
    int gid = get_global_id(0);
    if (gid >= 2 && gid < n - 2 && spike_mask[gid]) {
        // Linear interpolation
        signal[gid] = (signal[gid-1] + signal[gid+1]) * 0.5f;
    }
}
```

### Kernel Optimization Strategies

1. **Coalesced Memory Access**
   - Ensure consecutive threads access consecutive memory
   - Use appropriate work group sizes (64, 128, 256)

2. **Shared Memory (Local Memory)**
   - Cache frequently accessed data in local memory
   - Reduce global memory bandwidth

3. **Kernel Fusion**
   - Combine multiple operations into single kernel
   - Reduce kernel launch overhead
   - Keep data in registers/cache

4. **Vectorization**
   - Use `float2`, `float4`, `float8` types where appropriate
   - Process multiple samples per thread

5. **Async Execution**
   - Overlap CPU and GPU work
   - Use multiple command queues for concurrent execution
   - Pipeline data transfers

---

## Implementation Phases

### Phase 1: Foundation (Weeks 1-8)

**Goals**:
- Set up project structure and build system
- Implement core data structures
- Create GPU context manager with OpenCL
- Port filter definitions from CSV
- Basic I/O (read RF, write TBC)

**Deliverables**:
- CMakeLists.txt with all dependencies
- Working build on Linux, Windows, macOS
- Unit test framework with Catch2
- GPUContext class with device enumeration
- File I/O classes (RFReader, TBCWriter)
- Filter loading and management

**Success Criteria**:
- Can read raw RF file
- Can initialize OpenCL context
- Can load filter definitions
- All unit tests pass
- Cross-platform builds work

### Phase 2: RF Processing (Weeks 9-16)

**Goals**:
- Implement FFT engine (clFFT + FFTW fallback)
- RF filtering in frequency domain
- Hilbert transform
- Basic block processing pipeline

**Deliverables**:
- FFTEngine class with GPU and CPU paths
- FilterBank with frequency-domain filtering
- HilbertTransform implementation
- RFProcessor orchestrating the pipeline
- Benchmarks comparing to Python version

**Success Criteria**:
- Can process RF blocks with FFT filtering
- Hilbert transform matches Python output (within tolerance)
- GPU path is faster than CPU path
- Memory usage is reasonable

### Phase 3: FM Demodulation (Weeks 17-24)

**Goals**:
- Phase unwrapping kernel (critical for performance)
- Envelope detection
- Spike detection and replacement
- Video/chroma separation

**Deliverables**:
- PhaseUnwrapper with OpenCL kernel
- EnvelopeDetector with IIR filtering
- SpikeReplacer
- FMDemodulator orchestrating components
- Performance profiling tools

**Success Criteria**:
- Phase unwrapping matches Python output
- Envelope detection handles IIR filters
- Spike replacement works correctly
- Demodulation output bit-accurate to Python (or very close)

### Phase 4: Video Processing (Weeks 25-32)

**Goals**:
- Chroma separation and heterodyning
- Time-base correction
- Dropout detection
- Line sync detection

**Deliverables**:
- ChromaSeparator for all supported formats
- TimeBaseCorrector
- DropoutDetector
- SyncDetector
- VideoProcessor orchestrating components

**Success Criteria**:
- Chroma output matches Python decoder
- TBC output is valid and viewable
- Dropouts are correctly detected
- Works for all major formats (VHS, SVHS, Betamax)

### Phase 5: Integration & Optimization (Weeks 33-40)

**Goals**:
- Full end-to-end pipeline
- Multi-threaded block processing
- Advanced GPU optimizations
- Memory pooling and reuse

**Deliverables**:
- Complete VHSDecoder class
- Thread pool for parallel processing
- Memory pool for GPU buffers
- CLI application matching Python features
- Performance benchmarks

**Success Criteria**:
- Can decode full tapes
- Matches Python output quality
- 5-10x faster than Python GPU version
- Handles all supported formats

### Phase 6: Testing & Validation (Weeks 41-48)

**Goals**:
- Comprehensive test suite
- Validation against reference captures
- Bug fixing and refinement
- Documentation

**Deliverables**:
- Unit tests for all components
- Integration tests with reference data
- Performance regression tests
- User documentation
- API documentation (Doxygen)

**Success Criteria**:
- All tests pass on Linux, Windows, macOS
- Output matches Python decoder (validated with test suite)
- No known critical bugs
- Complete documentation

### Phase 7: Advanced Features (Weeks 49-56)

**Goals**:
- Format-specific optimizations
- Advanced TBC features
- GUI integration (optional)
- HiFi audio support

**Deliverables**:
- Optimized paths for each tape format
- Field averaging, EQ, and other advanced features
- Optional Qt-based GUI
- HiFi FM audio demodulation

**Success Criteria**:
- Feature parity with Python version
- Excellent performance on all formats
- Professional-grade tool ready for users

### Phase 8: Release & Maintenance (Weeks 57+)

**Goals**:
- Public release
- Continuous integration
- User support
- Bug fixes and improvements

**Deliverables**:
- Binary releases for all platforms
- Installation packages/installers
- CI/CD pipeline (GitHub Actions)
- Issue tracker management

---

## Performance Targets

### Current Python Performance (Baseline)

| Metric | CPU (8 threads) | GPU (CuPy) |
|--------|----------------|------------|
| Speed | 2.19 FPS | 2.49 FPS |
| Speedup | 1.0x | 1.14x |
| Memory | ~500 MB | ~600 MB |

### Target C++/OpenCL Performance

| Component | Target Speedup | Rationale |
|-----------|---------------|-----------|
| FFT Operations | 8-12x | clFFT highly optimized |
| Phase Unwrapping | 5-10x | Custom parallel kernel |
| Envelope Detection | 6-10x | Fused operations |
| Chroma Processing | 4-8x | Batch processing |
| Overall Pipeline | 5-20x | Combined optimizations |

**Conservative Target**: 12.5 FPS (5x speedup)  
**Optimistic Target**: 50 FPS (20x speedup)  
**Realistic Target**: 15-25 FPS (6-10x speedup)

### Memory Targets

- **Peak GPU Memory**: < 100 MB (current: 16.83 MB)
- **CPU Memory**: < 200 MB (current: ~500 MB)
- **Zero-copy I/O**: Where possible with memory mapping

---

## Code Organization

### Directory Structure

```
vhs-decode/
├── src/
│   ├── core/
│   │   ├── types.hpp           # Common types and structures
│   │   ├── config.hpp/cpp      # Configuration management
│   │   └── logger.hpp/cpp      # Logging infrastructure
│   │
│   ├── rf/
│   │   ├── rf_processor.hpp/cpp
│   │   ├── fft_engine.hpp/cpp
│   │   ├── filter_bank.hpp/cpp
│   │   └── hilbert.hpp/cpp
│   │
│   ├── demod/
│   │   ├── fm_demodulator.hpp/cpp
│   │   ├── phase_unwrap.hpp/cpp
│   │   ├── envelope.hpp/cpp
│   │   └── spike_replace.hpp/cpp
│   │
│   ├── video/
│   │   ├── video_processor.hpp/cpp
│   │   ├── chroma_separator.hpp/cpp
│   │   ├── tbc.hpp/cpp
│   │   ├── dropout_detector.hpp/cpp
│   │   └── sync_detector.hpp/cpp
│   │
│   ├── gpu/
│   │   ├── gpu_context.hpp/cpp
│   │   ├── memory_pool.hpp/cpp
│   │   ├── kernel_manager.hpp/cpp
│   │   └── profiler.hpp/cpp
│   │
│   ├── io/
│   │   ├── rf_reader.hpp/cpp
│   │   ├── tbc_writer.hpp/cpp
│   │   └── json_writer.hpp/cpp
│   │
│   ├── formats/
│   │   ├── format_base.hpp
│   │   ├── vhs.hpp/cpp
│   │   ├── svhs.hpp/cpp
│   │   ├── betamax.hpp/cpp
│   │   └── video8.hpp/cpp
│   │
│   └── main.cpp                # CLI application
│
├── kernels/
│   ├── phase_unwrap.cl
│   ├── envelope.cl
│   ├── chroma.cl
│   ├── spike.cl
│   └── utils.cl
│
├── tests/
│   ├── unit/
│   │   ├── test_fft.cpp
│   │   ├── test_phase_unwrap.cpp
│   │   ├── test_envelope.cpp
│   │   └── ...
│   │
│   ├── integration/
│   │   ├── test_full_decode.cpp
│   │   └── test_formats.cpp
│   │
│   └── benchmarks/
│       ├── bench_fft.cpp
│       ├── bench_demod.cpp
│       └── bench_full.cpp
│
├── tools/
│   ├── filter_converter/       # Convert Python filters to C++
│   ├── kernel_compiler/        # Pre-compile OpenCL kernels
│   └── reference_validator/    # Validate against Python output
│
├── data/
│   ├── filters/                # Filter coefficients (CSV)
│   ├── test_captures/          # Reference RF captures
│   └── expected_outputs/       # Expected TBC outputs
│
├── docs/
│   ├── API.md
│   ├── ARCHITECTURE.md
│   ├── OPENCL_GUIDE.md
│   └── PORTING_GUIDE.md
│
└── CMakeLists.txt
```

### Header-Only vs. Compiled

**Header-Only** (for templates and small utilities):
- `types.hpp` - Common types
- `span.hpp` - C++20 span backport (if needed)
- Template classes

**Compiled** (for large implementations):
- All core processing classes
- OpenCL kernel management
- I/O operations

---

## Dependency Management

### Required Dependencies

| Library | Purpose | License | Notes |
|---------|---------|---------|-------|
| OpenCL | GPU acceleration | Khronos | ICD loader + headers |
| clFFT | FFT on GPU | Apache 2.0 | Can be bundled |
| FFTW3 | CPU FFT fallback | GPL v2+ | Or use Pocket FFT (MIT) |
| Eigen3 | Linear algebra | MPL2 | Header-only option |
| nlohmann/json | JSON I/O | MIT | Header-only |
| spdlog | Logging | MIT | Header-only option |
| CLI11 | CLI parsing | BSD-3 | Header-only |
| Catch2 | Testing | BSL-1.0 | Can use CMake fetch |

### Optional Dependencies

| Library | Purpose | License | Notes |
|---------|---------|---------|-------|
| Intel TBB | Advanced threading | Apache 2.0 | Better than std::thread pool |
| Intel MKL | Optimized FFT/BLAS | Intel EULA | For Intel CPUs |
| Qt6 | GUI (optional) | LGPL/GPL | Already used in project |
| CUDA | Alternative GPU backend | Proprietary | NVIDIA-only |

### Dependency Installation

**Via vcpkg** (recommended):
```bash
vcpkg install opencl clfft eigen3 nlohmann-json spdlog cli11 catch2
```

**Via system package manager**:
```bash
# Ubuntu/Debian
sudo apt install opencl-headers ocl-icd-opencl-dev libfftw3-dev \
                 libeigen3-dev nlohmann-json3-dev

# Fedora
sudo dnf install opencl-headers ocl-icd-devel fftw-devel \
                 eigen3-devel json-devel

# macOS
brew install opencl-headers fftw eigen nlohmann-json
```

---

## Testing Strategy

### Unit Tests

**FFT Engine**:
- Forward/inverse transform accuracy
- Different block sizes
- Complex/real transforms
- CPU vs. GPU consistency

**Phase Unwrapping**:
- Known test signals (swept frequency)
- Comparison with Python implementation
- Edge cases (discontinuities)

**Envelope Detection**:
- IIR filter accuracy
- Comparison with scipy.signal.sosfiltfilt
- Different filter orders

**Chroma Processing**:
- Heterodyne accuracy for each format
- Frequency shifting correctness
- Bandpass filtering

### Integration Tests

**Full Decode**:
- Reference RF capture → TBC output
- Compare with Python decoder output
- Bit-accurate or within tolerance

**Format Tests**:
- VHS NTSC/PAL
- SVHS NTSC/PAL
- Betamax NTSC/PAL
- Video8/Hi8
- U-Matic

### Performance Tests

**Benchmarks**:
- FFT throughput (samples/sec)
- Demodulation speed (blocks/sec)
- Full decode speed (FPS)
- Memory usage (peak, average)

**Regression Tests**:
- Track performance over time
- Alert on significant slowdowns
- Compare against baselines

### Validation Tests

**Output Validation**:
- TBC format correctness
- JSON metadata completeness
- Chroma data accuracy
- Dropout detection accuracy

**Visual Tests**:
- Export TBC to video
- Visual inspection for artifacts
- Comparison with Python output

---

## Migration Strategy

### Gradual Migration Path

Instead of rewriting everything at once, we can migrate incrementally:

**Option 1: Hybrid Approach**
- Keep Python CLI and high-level logic
- Replace performance-critical components with C++
- Use Python C API or pybind11 for integration
- Gradually replace more components

**Option 2: Clean Rewrite** (recommended)
- Build C++ version alongside Python
- Maintain both during transition
- Validate C++ output against Python
- Eventually deprecate Python version

**Option 3: Coexistence**
- Keep Python for development/prototyping
- Use C++ for production decoding
- Share filter definitions and metadata formats

### Python Interoperability (if needed)

```cpp
// pybind11 example wrapper
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

py::array_t<float> process_block_py(py::array_t<uint8_t> rf_data) {
    auto buf = rf_data.request();
    auto* ptr = static_cast<uint8_t*>(buf.ptr);
    
    // Process with C++ code
    RFProcessor processor(config);
    auto result = processor.processBlock({ptr, buf.size});
    
    // Return as NumPy array
    return py::array_t<float>(result.size(), result.data());
}

PYBIND11_MODULE(vhsdecode_cpp, m) {
    m.def("process_block", &process_block_py);
}
```

---

## Risk Assessment & Mitigation

### Technical Risks

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| OpenCL compatibility issues | High | Medium | Extensive testing, CPU fallback |
| Phase unwrapping accuracy | High | Medium | Rigorous validation, reference tests |
| Performance not meeting targets | Medium | Low | Profile early, optimize iteratively |
| Memory leaks/corruption | High | Low | RAII, smart pointers, Valgrind |
| Cross-platform bugs | Medium | Medium | CI on all platforms, extensive testing |

### Project Risks

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Scope creep | High | Medium | Strict phase definitions, MVP focus |
| Insufficient testing data | High | Low | Use existing test suite, generate more |
| Developer availability | Medium | Medium | Clear documentation, modular design |
| Breaking changes in format specs | Low | Low | Abstract format definitions |

### Mitigation Strategies

1. **Continuous Validation**
   - Test against Python version at every milestone
   - Maintain reference test suite
   - Automated regression testing

2. **Performance Monitoring**
   - Benchmark after each major change
   - Profile before optimizing
   - Track metrics over time

3. **Code Quality**
   - Modern C++ best practices
   - Comprehensive code reviews
   - Static analysis (clang-tidy, cppcheck)
   - Dynamic analysis (Valgrind, AddressSanitizer)

4. **Documentation**
   - Inline code documentation
   - Architecture documentation
   - User guides
   - API reference (Doxygen)

---

## Success Metrics

### Performance Metrics

- **Speed**: 6-10x faster than Python GPU version (target: 15-25 FPS)
- **Memory**: < 200 MB peak usage
- **Accuracy**: Output matches Python within 0.1% (or bit-accurate where possible)
- **Compatibility**: Works on 95%+ of systems with OpenCL 1.2+

### Quality Metrics

- **Test Coverage**: > 80% code coverage
- **Bug Density**: < 1 critical bug per 1000 LOC
- **Build Success**: 100% on CI (Linux, Windows, macOS)
- **Documentation**: All public APIs documented

### User Metrics

- **Installation**: < 5 minutes from download to first use
- **Learning Curve**: Existing users can switch with minimal retraining
- **Reliability**: < 1 crash per 100 hours of decoding
- **Performance**: Decode tapes in real-time (1x speed) on mid-range GPU

---

## Alternative Approaches

### 1. Optimize Python Further

**Pros**:
- Less development effort
- Maintains current codebase
- Familiar to current developers

**Cons**:
- Limited performance gains (2-3x max)
- Still requires Python runtime
- GIL limits multi-threading
- Memory overhead of Python

**Verdict**: Not recommended for 10x+ performance goals

### 2. Use CUDA Instead of OpenCL

**Pros**:
- Potentially better performance on NVIDIA GPUs
- More mature ecosystem (cuFFT, Thrust, etc.)
- Better tooling (Nsight)

**Cons**:
- NVIDIA-only (no AMD, Intel support)
- Vendor lock-in
- Current Python code already uses CuPy

**Verdict**: Could be optional backend, but OpenCL should be primary

### 3. Use Vulkan Compute

**Pros**:
- Modern API, better performance potential
- Cross-vendor support
- Future-proof

**Cons**:
- More complex than OpenCL
- Less mature compute ecosystem
- Steeper learning curve

**Verdict**: Consider for Phase 8+ as optional backend

### 4. Hybrid Python/C++

**Pros**:
- Gradual migration
- Can test C++ components incrementally
- Lower risk

**Cons**:
- Overhead of Python/C++ boundary
- More complex codebase
- May not achieve full performance potential

**Verdict**: Good for prototyping, but full C++ is better long-term

---

## Conclusion

This C++/OpenCL rewrite plan provides a comprehensive roadmap for transforming VHS-Decode into a high-performance, cross-platform native application. The phased approach minimizes risk while delivering incremental value, and the use of OpenCL ensures broad GPU support.

**Key Takeaways**:

1. **Achievable Performance**: 5-20x speedup is realistic with proper implementation
2. **Cross-Platform**: OpenCL works on NVIDIA, AMD, Intel GPUs
3. **Maintainable**: Modern C++ with clear architecture
4. **Tested**: Comprehensive testing strategy ensures quality
5. **Incremental**: Phased development reduces risk

**Next Steps**:

1. Approve this plan and allocate resources
2. Set up development environment and infrastructure
3. Begin Phase 1: Foundation
4. Establish CI/CD pipeline
5. Create initial test suite with reference data

**Timeline Summary**:
- **Phase 1-2** (Weeks 1-16): Foundation + RF Processing
- **Phase 3-4** (Weeks 17-32): Demodulation + Video Processing
- **Phase 5-6** (Weeks 33-48): Integration + Testing
- **Phase 7-8** (Weeks 49+): Advanced Features + Release

**Total Estimated Time**: 12-18 months to production-ready v1.0

---

## Appendix A: Example Code Snippets

### Main Decode Function

```cpp
int main(int argc, char* argv[]) {
    // Parse command line
    CLI::App app{"VHS-Decode - RF tape decoder"};
    
    std::string inputFile;
    std::string outputFile;
    std::string format = "VHS";
    std::string system = "NTSC";
    bool useGPU = true;
    int threads = 4;
    
    app.add_option("-i,--input", inputFile, "Input RF file")
        ->required();
    app.add_option("-o,--output", outputFile, "Output TBC base name")
        ->required();
    app.add_option("-f,--format", format, "Tape format (VHS, SVHS, etc.)");
    app.add_option("-s,--system", system, "TV system (NTSC, PAL)");
    app.add_flag("-g,--gpu", useGPU, "Use GPU acceleration");
    app.add_option("-t,--threads", threads, "Number of threads");
    
    CLI11_PARSE(app, argc, argv);
    
    try {
        // Initialize GPU context
        GPUContext::Config gpuConfig;
        gpuConfig.enableGPU = useGPU;
        auto gpu = std::make_unique<GPUContext>(gpuConfig);
        
        if (useGPU && !gpu->isGPUAvailable()) {
            spdlog::warn("GPU not available, falling back to CPU");
            useGPU = false;
        }
        
        // Create decoder
        VHSDecoder::Config config;
        config.format = format;
        config.system = system;
        config.threads = threads;
        config.inputFreq = 40.0;  // 40 MSPS
        
        VHSDecoder decoder(config, gpu.get());
        
        // Open input file
        RFReader reader(inputFile);
        
        // Open output file
        TBCWriter writer(outputFile);
        
        // Progress bar
        auto progressCallback = [](size_t current, size_t total) {
            std::cout << "\rProgress: " << current << "/" << total 
                     << " fields (" << (100 * current / total) << "%)" 
                     << std::flush;
        };
        writer.setProgressCallback(progressCallback);
        
        // Decode loop
        spdlog::info("Starting decode...");
        auto startTime = std::chrono::steady_clock::now();
        
        size_t fieldCount = 0;
        while (auto rfBlock = reader.readBlock()) {
            auto field = decoder.decodeField(*rfBlock);
            writer.writeField(field);
            fieldCount++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            endTime - startTime).count();
        
        // Report statistics
        double fps = fieldCount / static_cast<double>(duration);
        spdlog::info("Decoded {} fields in {} seconds ({:.2f} FPS)", 
                    fieldCount, duration, fps);
        
        if (useGPU) {
            auto stats = gpu->getStats();
            spdlog::info("Peak GPU memory: {:.2f} MB", 
                        stats.peakMemoryMB);
        }
        
        writer.close();
        spdlog::info("Decode complete!");
        
    } catch (const std::exception& e) {
        spdlog::error("Decode failed: {}", e.what());
        return 1;
    }
    
    return 0;
}
```

### FFT Engine Implementation

```cpp
class FFTEngine {
public:
    FFTEngine(GPUContext* gpu, size_t blockSize)
        : gpu_(gpu), blockSize_(blockSize)
    {
        if (gpu && gpu->isGPUAvailable()) {
            initializeGPU();
        } else {
            initializeCPU();
        }
    }
    
    ~FFTEngine() {
        cleanup();
    }
    
    ComplexArray forward(const RealArray& input) {
        if (useGPU_) {
            return forwardGPU(input);
        } else {
            return forwardCPU(input);
        }
    }
    
    RealArray inverse(const ComplexArray& input) {
        if (useGPU_) {
            return inverseGPU(input);
        } else {
            return inverseCPU(input);
        }
    }

private:
    void initializeGPU() {
        try {
            clfftSetupData setupData;
            clfftInitSetupData(&setupData);
            clfftSetup(&setupData);
            
            size_t dims[1] = {blockSize_};
            clfftCreateDefaultPlan(&fftPlan_, gpu_->getContext()(),
                                  CLFFT_1D, dims);
            
            clfftSetPlanPrecision(fftPlan_, CLFFT_SINGLE);
            clfftSetLayout(fftPlan_, CLFFT_REAL, CLFFT_HERMITIAN_INTERLEAVED);
            clfftSetResultLocation(fftPlan_, CLFFT_OUTOFPLACE);
            
            cl_command_queue queue = gpu_->getQueue()();
            clfftBakePlan(fftPlan_, 1, &queue, nullptr, nullptr);
            
            useGPU_ = true;
            spdlog::info("FFT engine initialized on GPU");
            
        } catch (const std::exception& e) {
            spdlog::warn("GPU FFT init failed: {}, falling back to CPU", 
                        e.what());
            initializeCPU();
        }
    }
    
    void initializeCPU() {
        // Initialize FFTW3
        fftwPlan_ = fftwf_plan_dft_r2c_1d(
            blockSize_, nullptr, nullptr, FFTW_ESTIMATE);
        
        useGPU_ = false;
        spdlog::info("FFT engine initialized on CPU");
    }
    
    ComplexArray forwardGPU(const RealArray& input) {
        // Transfer to GPU
        cl::Buffer inputBuf = gpu_->allocate<float>(
            input.size(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
        
        // Allocate output buffer
        size_t outputSize = blockSize_ / 2 + 1;
        cl::Buffer outputBuf = gpu_->allocate<std::complex<float>>(
            outputSize, CL_MEM_WRITE_ONLY);
        
        // Execute FFT
        cl_command_queue queue = gpu_->getQueue()();
        clfftEnqueueTransform(fftPlan_, CLFFT_FORWARD, 1, &queue,
                             0, nullptr, nullptr,
                             &inputBuf(), &outputBuf(), nullptr);
        
        // Read back result
        ComplexArray output(outputSize);
        gpu_->getQueue().enqueueReadBuffer(outputBuf, CL_TRUE, 0,
            outputSize * sizeof(std::complex<float>), output.data());
        
        return output;
    }
    
    ComplexArray forwardCPU(const RealArray& input) {
        // Use FFTW
        auto output = std::make_unique<fftwf_complex[]>(blockSize_ / 2 + 1);
        fftwf_execute_dft_r2c(fftwPlan_,
            const_cast<float*>(input.data()),
            output.get());
        
        // Convert to std::complex
        ComplexArray result(blockSize_ / 2 + 1);
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] = std::complex<float>(output[i][0], output[i][1]);
        }
        
        return result;
    }
    
    // Similar for inverseGPU and inverseCPU...
    
    void cleanup() {
        if (useGPU_ && fftPlan_) {
            clfftDestroyPlan(&fftPlan_);
            clfftTeardown();
        }
        if (fftwPlan_) {
            fftwf_destroy_plan(fftwPlan_);
        }
    }
    
    GPUContext* gpu_;
    size_t blockSize_;
    bool useGPU_ = false;
    
    // GPU resources
    clfftPlanHandle fftPlan_ = 0;
    
    // CPU resources
    fftwf_plan fftwPlan_ = nullptr;
};
```

---

## Appendix B: Build Instructions

### Linux

```bash
# Install dependencies
sudo apt install cmake build-essential opencl-headers ocl-icd-opencl-dev \
                 libfftw3-dev libeigen3-dev nlohmann-json3-dev

# Clone repository
git clone https://github.com/oyvindln/vhs-decode.git
cd vhs-decode

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Install
sudo cmake --install .

# Run
vhs-decode -i input.r40 -o output.tbc --system PAL --format VHS
```

### Windows (with vcpkg)

```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies
.\vcpkg install opencl clfft fftw3 eigen3 nlohmann-json spdlog cli11 catch2

# Build
cd vhs-decode
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Run
Release\vhs-decode.exe -i input.r40 -o output.tbc
```

### macOS

```bash
# Install dependencies
brew install cmake opencl-headers fftw eigen nlohmann-json spdlog cli11 catch2

# Build (same as Linux)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Run
./vhs-decode -i input.r40 -o output.tbc
```

---

## Appendix C: References

### OpenCL Resources
- [OpenCL Programming Guide](https://www.khronos.org/opencl/)
- [clFFT Documentation](https://github.com/clMathLibraries/clFFT)
- [OpenCL Best Practices](https://www.khronos.org/files/opencl-quick-reference-card.pdf)

### Signal Processing
- [FFT Algorithms](https://en.wikipedia.org/wiki/Fast_Fourier_transform)
- [Phase Unwrapping](https://en.wikipedia.org/wiki/Phase_unwrapping)
- [FM Demodulation](https://en.wikipedia.org/wiki/Frequency_modulation)
- [Hilbert Transform](https://en.wikipedia.org/wiki/Hilbert_transform)

### C++ Resources
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Effective Modern C++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition)

### VHS-Decode Specific
- [Current Python Implementation](https://github.com/oyvindln/vhs-decode)
- [LD-Decode Wiki](https://github.com/happycube/ld-decode/wiki)
- [TBC Format Specification](https://github.com/oyvindln/vhs-decode/wiki)

---

**Document End**

This plan is a living document and will be updated as the project progresses.
For questions or suggestions, please open an issue on GitHub.
