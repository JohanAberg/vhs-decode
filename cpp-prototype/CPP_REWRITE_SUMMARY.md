# VHS-Decode C++/OpenCL Rewrite - Executive Summary

> **Full detailed plan**: See [CPP_OPENCL_REWRITE_PLAN.md](./CPP_OPENCL_REWRITE_PLAN.md)

---

## Why Rewrite in C++/OpenCL?

### Current State
- **Language**: Python (~36,000 lines) with NumPy/SciPy/CuPy
- **Performance**: 2.49 FPS (GPU), 2.19 FPS (8-thread CPU)
- **Limitations**: 
  - Python overhead and GIL
  - CuPy is CUDA-only (NVIDIA-only)
  - Large memory footprint (~500-600 MB)
  - Deployment requires Python runtime

### Target State
- **Language**: C++17/20 with OpenCL
- **Performance**: 15-25 FPS (6-10x speedup, conservative estimate)
- **Benefits**:
  - ✅ Cross-platform GPU (NVIDIA, AMD, Intel)
  - ✅ Native performance without Python overhead
  - ✅ Lower memory footprint (~200 MB)
  - ✅ Standalone executables
  - ✅ Professional-grade reliability

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      VHS-Decode CLI                         │
│                   (Command-line Interface)                  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                     VHSDecoder Controller                   │
│              (Orchestrates entire decode pipeline)          │
└──────┬────────────────┬──────────────────┬──────────────────┘
       │                │                  │
       ▼                ▼                  ▼
┌─────────────┐  ┌──────────────┐  ┌─────────────────┐
│ RFProcessor │  │ FMDemodulator│  │ VideoProcessor  │
│             │  │              │  │                 │
│ • FFT       │  │ • Phase      │  │ • Chroma Sep    │
│ • Filtering │  │   Unwrap     │  │ • TBC           │
│ • Hilbert   │  │ • Envelope   │  │ • Dropouts      │
│             │  │ • Spikes     │  │ • Sync Detect   │
└──────┬──────┘  └──────┬───────┘  └──────┬──────────┘
       │                │                  │
       └────────────────┴──────────────────┘
                        │
                        ▼
         ┌───────────────────────────┐
         │      GPU Context          │
         │   (OpenCL Management)     │
         │                           │
         │  • Device Selection       │
         │  • Kernel Compilation     │
         │  • Memory Pooling         │
         │  • CPU Fallback           │
         └───────────────────────────┘
```

---

## Performance Comparison

| Metric | Python CPU | Python GPU (CuPy) | C++ Target | Improvement |
|--------|------------|-------------------|------------|-------------|
| **Speed** | 2.19 FPS | 2.49 FPS | 15-25 FPS | **6-10x** |
| **Memory** | ~500 MB | ~600 MB | ~200 MB | **2-3x less** |
| **GPU Support** | N/A | NVIDIA only | All vendors | Universal |
| **Deployment** | Python + deps | Python + CUDA | Single binary | Simple |
| **Multi-threading** | Limited (GIL) | Limited | Native | Full CPU usage |

---

## Technology Stack

### Core Technologies

| Component | Choice | Why? |
|-----------|--------|------|
| **Language** | C++17/20 | Modern features, performance, portability |
| **GPU API** | OpenCL 1.2+ | Cross-vendor (NVIDIA/AMD/Intel) |
| **FFT** | clFFT + FFTW3 | GPU + CPU fallback |
| **Linear Algebra** | Eigen3 | Header-only, fast, SIMD |
| **JSON** | nlohmann/json | Modern, easy to use |
| **Logging** | spdlog | Fast, structured logging |
| **CLI** | CLI11 | Modern argument parsing |
| **Testing** | Catch2 | Expressive, modern |
| **Build** | CMake 3.16+ | Industry standard |

### Optional Enhancements

- **Intel TBB**: Advanced task scheduling
- **Intel MKL**: Optimized BLAS/FFT for Intel CPUs
- **Vulkan Compute**: Future GPU backend option
- **CUDA**: Alternative NVIDIA backend

---

## Implementation Roadmap (12-18 Months)

### Phase 1: Foundation (Weeks 1-8) 🏗️
**Goal**: Set up infrastructure
- ✅ CMake build system for all platforms
- ✅ OpenCL context and device management
- ✅ File I/O (RF reader, TBC writer)
- ✅ Unit test framework
- ✅ Filter loading from CSV

**Deliverable**: Can read RF, initialize GPU, run tests

---

### Phase 2: RF Processing (Weeks 9-16) 🔧
**Goal**: Core signal processing
- ✅ FFT engine (clFFT for GPU, FFTW3 for CPU)
- ✅ Frequency-domain filtering
- ✅ Hilbert transform
- ✅ Block processing pipeline

**Deliverable**: Can process RF blocks with FFT filtering

---

### Phase 3: FM Demodulation (Weeks 17-24) 📡
**Goal**: Demodulate FM signal
- ✅ Phase unwrapping (OpenCL kernel)
- ✅ Envelope detection with IIR filtering
- ✅ Spike detection and replacement
- ✅ Video/chroma separation

**Deliverable**: FM demodulation output matches Python

---

### Phase 4: Video Processing (Weeks 25-32) 📺
**Goal**: Complete video pipeline
- ✅ Chroma separation (heterodyning)
- ✅ Time-base correction
- ✅ Dropout detection
- ✅ Line sync detection

**Deliverable**: Valid TBC output for all formats

---

### Phase 5: Integration & Optimization (Weeks 33-40) ⚡
**Goal**: Full pipeline and performance
- ✅ End-to-end decoder
- ✅ Multi-threaded block processing
- ✅ GPU memory pooling
- ✅ Performance optimizations

**Deliverable**: 5-10x faster than Python GPU

---

### Phase 6: Testing & Validation (Weeks 41-48) ✔️
**Goal**: Quality assurance
- ✅ Comprehensive test suite
- ✅ Validation against reference captures
- ✅ Cross-platform testing
- ✅ Documentation

**Deliverable**: Production-ready, tested code

---

### Phase 7: Advanced Features (Weeks 49-56) 🎨
**Goal**: Feature parity with Python
- ✅ Format-specific optimizations
- ✅ Field averaging, EQ
- ✅ Optional GUI
- ✅ HiFi audio support

**Deliverable**: Complete feature set

---

### Phase 8: Release & Maintenance (Weeks 57+) 🚀
**Goal**: Public release
- ✅ Binary releases (Linux, Windows, macOS)
- ✅ CI/CD pipeline
- ✅ User documentation
- ✅ Community support

**Deliverable**: Production v1.0

---

## Critical OpenCL Kernels

### 1. Phase Unwrapping (Highest Impact)
```
Performance: 46% of decode time (Python GPU)
Expected Speedup: 5-10x
Strategy: Parallel phase difference + prefix sum scan
```

### 2. Envelope Detection
```
Performance: 22% of decode time (Python GPU)
Expected Speedup: 6-10x
Strategy: Fused abs + IIR filtering on GPU
```

### 3. FFT Operations
```
Performance: 15-20% of decode time
Expected Speedup: 8-12x
Strategy: Use highly optimized clFFT library
```

### 4. Chroma Heterodyning
```
Performance: 11% of decode time
Expected Speedup: 4-8x
Strategy: Batch frequency shifting on GPU
```

### 5. Spike Detection/Replacement
```
Performance: 5-10% of decode time
Expected Speedup: 3-6x
Strategy: Parallel detection + interpolation
```

---

## Code Organization

```
vhs-decode/
│
├── src/
│   ├── core/        # Common types, config, logging
│   ├── rf/          # RF processing, FFT, filters, Hilbert
│   ├── demod/       # FM demodulation, phase unwrap, envelope
│   ├── video/       # Video processing, chroma, TBC, dropouts
│   ├── gpu/         # OpenCL context, kernels, memory management
│   ├── io/          # File I/O (RF reader, TBC writer, JSON)
│   ├── formats/     # Format-specific code (VHS, SVHS, Betamax, etc.)
│   └── main.cpp     # CLI application entry point
│
├── kernels/         # OpenCL kernel source files (.cl)
│   ├── phase_unwrap.cl
│   ├── envelope.cl
│   ├── chroma.cl
│   ├── spike.cl
│   └── utils.cl
│
├── tests/
│   ├── unit/        # Unit tests for individual components
│   ├── integration/ # Full decode tests
│   └── benchmarks/  # Performance benchmarks
│
├── tools/           # Utilities
│   ├── filter_converter/    # Convert Python filters to C++
│   ├── kernel_compiler/     # Pre-compile OpenCL kernels
│   └── reference_validator/ # Validate against Python output
│
├── data/
│   ├── filters/            # Filter coefficients (CSV)
│   ├── test_captures/      # Reference RF captures
│   └── expected_outputs/   # Expected TBC outputs
│
├── docs/
│   ├── API.md
│   ├── ARCHITECTURE.md
│   ├── OPENCL_GUIDE.md
│   └── PORTING_GUIDE.md
│
└── CMakeLists.txt
```

---

## Testing Strategy

### Unit Tests
- **FFT Engine**: Accuracy, different sizes, CPU vs GPU consistency
- **Phase Unwrapping**: Known signals, edge cases
- **Envelope Detection**: IIR filter accuracy vs scipy
- **Chroma Processing**: Heterodyne accuracy for each format

### Integration Tests
- **Full Decode**: Reference captures → TBC output
- **Format Tests**: VHS, SVHS, Betamax, Video8, U-Matic
- **Edge Cases**: Dropouts, sync issues, noise

### Performance Tests
- **Benchmarks**: FFT throughput, demod speed, full decode FPS
- **Regression**: Track performance over time
- **Memory**: Peak usage, leak detection

### Validation Tests
- **Output Validation**: TBC format, JSON metadata, chroma
- **Visual Tests**: Export to video, visual inspection
- **Comparison**: Python output vs C++ output

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| **OpenCL compatibility** | Extensive testing + CPU fallback |
| **Phase unwrap accuracy** | Rigorous validation with reference tests |
| **Performance targets** | Profile early, optimize iteratively |
| **Cross-platform bugs** | CI on Linux, Windows, macOS |
| **Scope creep** | Strict phase definitions, MVP focus |
| **Insufficient test data** | Use existing test suite, generate more |

---

## Success Metrics

### Performance
- ✅ **Speed**: 15-25 FPS (6-10x faster than Python GPU)
- ✅ **Memory**: < 200 MB peak usage
- ✅ **Accuracy**: Output matches Python within 0.1%
- ✅ **Compatibility**: Works on 95%+ systems with OpenCL 1.2+

### Quality
- ✅ **Test Coverage**: > 80%
- ✅ **Bug Density**: < 1 critical bug per 1000 LOC
- ✅ **Build Success**: 100% on CI
- ✅ **Documentation**: All public APIs documented

### User Experience
- ✅ **Installation**: < 5 minutes
- ✅ **Learning Curve**: Minimal for existing users
- ✅ **Reliability**: < 1 crash per 100 hours
- ✅ **Real-time**: Decode at 1x speed on mid-range GPU

---

## Build Instructions Quick Start

### Linux
```bash
# Install dependencies
sudo apt install cmake build-essential opencl-headers ocl-icd-opencl-dev \
                 libfftw3-dev libeigen3-dev nlohmann-json3-dev

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Run
./vhs-decode -i input.r40 -o output.tbc --system PAL --format VHS
```

### Windows (with vcpkg)
```powershell
# Install dependencies via vcpkg
.\vcpkg install opencl clfft fftw3 eigen3 nlohmann-json spdlog cli11 catch2

# Build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Run
Release\vhs-decode.exe -i input.r40 -o output.tbc
```

### macOS
```bash
# Install dependencies
brew install cmake opencl-headers fftw eigen nlohmann-json

# Build (same as Linux)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Run
./vhs-decode -i input.r40 -o output.tbc
```

---

## Next Steps

1. **Review & Approve** this plan
2. **Allocate Resources** (developer time, hardware for testing)
3. **Set Up Infrastructure**
   - Git repository structure
   - CI/CD pipeline (GitHub Actions)
   - Test data repository
4. **Begin Phase 1** (Foundation)
   - CMake project setup
   - OpenCL context implementation
   - Basic I/O classes
5. **Establish Testing**
   - Unit test framework
   - Reference test captures
   - Validation tools

---

## Questions?

- **Q: Why OpenCL and not CUDA?**
  - A: OpenCL works on NVIDIA, AMD, and Intel GPUs. CUDA is NVIDIA-only. We can add CUDA as optional backend later.

- **Q: Can we keep Python for some parts?**
  - A: Yes, hybrid approach is possible. We can use Python for CLI/scripting and C++ for performance-critical parts via pybind11.

- **Q: What if OpenCL isn't available?**
  - A: Full CPU fallback using FFTW3 and SIMD-optimized C++ code. Will still be faster than Python.

- **Q: How will we validate correctness?**
  - A: Compare output with current Python decoder using reference test captures. Automated tests will catch regressions.

- **Q: What about existing tools (ld-analyse, etc.)?**
  - A: They'll continue working. C++ decoder just replaces the Python RF→TBC stage. TBC format remains the same.

- **Q: Is 12-18 months realistic?**
  - A: Yes, assuming 1-2 full-time developers. Could be faster with more resources or slower if part-time.

---

## See Also

- **Full Plan**: [CPP_OPENCL_REWRITE_PLAN.md](./CPP_OPENCL_REWRITE_PLAN.md) - Complete technical details
- **Current Code**: [vhsdecode/](./vhsdecode/) - Python implementation
- **GPU Status**: [GPU_PROFILING_GUIDE.md](./GPU_PROFILING_GUIDE.md) - Current GPU performance

---

**Status**: Ready for review and approval  
**Last Updated**: December 19, 2025
