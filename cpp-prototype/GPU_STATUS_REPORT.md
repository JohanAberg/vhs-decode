# GPU Support Status Report (C++ Prototype)
Date: December 22, 2025

## Current Status
The C++ prototype has partial infrastructure for GPU acceleration via OpenCL, but the critical processing pipeline is currently running entirely on CPU.

### Implemented Components
1.  **FFT Engine (`src/rf/fft_engine.cpp`):**
    -   Has `HAVE_CLFFT` guards to support `clFFT`.
    -   Can initialize `CLFFTEngine` if `ENABLE_GPU` is set and `clFFT` is found.
2.  **Kernels (`src/gpu/kernels/`):**
    -   `phase_unwrap.cl`: Implements phase differencing (frequency extraction) and accumulation.
    -   `filter_apply.cl`: Likely for frequency domain filtering.
    -   `envelope.cl`: For envelope detection.

### Missing Components (Gaps)
1.  **FM Demodulator Integration:**
    -   `FMDemodulator` is currently CPU-only.
    -   It needs to use the `phase_diff` kernel to compute frequency from the analytic signal on GPU.
    -   It needs to implement the "Diff Demod" / "Spike Replacement" logic on GPU (currently CPU-only).
2.  **RF Processor Integration:**
    -   `RFProcessor` uses `FFTEngine`, so it *could* use GPU FFTs.
    -   However, the data transfer overhead (copying `std::vector` back and forth) between stages (FFT -> Filter -> IFFT -> Hilbert) would be prohibitive without keeping data in GPU memory.
    -   Need a `GPUBuffer` abstraction passed between stages instead of `std::vector`.
3.  **Hilbert Transform:**
    -   Currently implemented as a separate CPU class.
    -   Needs to be integrated into the GPU filter pipeline (can be combined with RF Bandpass).
4.  **Pipeline Orchestration:**
    -   The `main.cpp` loop pulls data to CPU (`rfResult.analyticSignal`) for FM demodulation.
    -   For full speed, the entire chain (RF -> Hilbert -> FM Demod -> Video Filter) should stay on GPU.

## Benchmarks (CPU Baseline)
**Test:** 10 frames, PAL, 4 threads (FFTW single-threaded).
**Total Time:** ~27.6s (0.36 FPS).

| Operation | Time per Block (ms) | % of Total | Notes |
|-----------|---------------------|------------|-------|
| **RF Process** | **36.80** | **65%** | **HOTSPOT**. Redundant FFTs (Bandpass + Hilbert). |
| FM Demod | 8.83 | 16% | |
| Convert/Buffer | 5.77 | 10% | |
| Video Filter | 4.61 | 8% | |
| Spike Replace | 0.46 | 1% | |
| Read Block | 0.04 | <1% | |

**Analysis:**
-   The C++ prototype is significantly slower than the Python reference (~2.2 FPS).
-   **Reason 1:** `RFProcessor` performs 4 FFTs per block (FFT->Filter->IFFT, then FFT->Hilbert->IFFT). This is redundant; Hilbert can be applied in the frequency domain of the first FFT.
-   **Reason 2:** FFTW3 is running single-threaded. Python's NumPy/SciPy likely uses multi-threaded MKL/OpenBLAS.

## Next Steps
1.  **Optimization (High Priority):**
    -   Combine RF Bandpass and Hilbert Transform in `RFProcessor` to reduce FFT count from 4 to 2 per block.
    -   Enable multi-threading in FFTW3.
2.  **GPU Pipeline:**
    -   Activate `clFFT` path.
    -   Implement `FMDemodulator` on GPU.
    -   Keep data on GPU between RF and Demod stages.
