# Modular Decoder Architecture Proposal

## Overview
To support a rich UI with real-time tuning capabilities, the decoder architecture will move from a monolithic processing loop to a **Pipeline of Configurable Stages**. Each stage exposes "Properties" that can be read/written by a UI layer, and "Ports" for data flow.

## Core Concepts

### 1. The Pipeline Context
A shared data structure passed between stages containing the signal in various states.
```cpp
struct PipelineContext {
    // Data Buffers (Smart Pointers to avoid copies)
    std::shared_ptr<RealArray> rfRaw;          // Raw RF samples
    std::shared_ptr<ComplexArray> rfAnalytic;  // Analytic Signal (after Hilbert)
    std::shared_ptr<RealArray> videoDemod;     // Demodulated Video (FM)
    std::shared_ptr<RealArray> videoFiltered;  // Filtered Video (LPF/De-emph)
    
    // Metadata
    size_t blockIndex;
    double sampleRate;
};
```

### 2. Configurable Properties
A generic interface for exposing parameters to a UI.
```cpp
struct Property {
    std::string name;
    std::string description;
    enum Type { FLOAT, INT, BOOL, ENUM } type;
    std::variant<float, int, bool, std::string> value;
    std::variant<float, int> min;
    std::variant<float, int> max;
    std::function<void(const Property&)> onChange;
};
```

## Pipeline Stages

### Stage 1: RF Source & Conditioning
**Responsibility:** Read raw data, convert to float, apply gain/offset.
**User Parameters:**
-   `Gain` (float): Digital gain adjustment.
-   `DC Offset` (float): Center the signal.

### Stage 2: RF Filtering & Analytic Conversion (The "RF Processor")
**Responsibility:** Bandpass filter the RF signal and convert to analytic signal (Hilbert).
**Optimization:** These two operations are mathematically fused into one frequency-domain multiplication.
**User Parameters:**
-   `Bandpass Low` (MHz): Critical for removing low-frequency noise/chroma.
    -   *Default:* 1.3 MHz (PAL), 1.5 MHz (NTSC).
-   `Bandpass High` (MHz): Limits bandwidth.
    -   *Default:* 6.0 MHz.
-   `Filter Order` (int): Steepness of the filter (Advanced).

### Stage 3: FM Demodulation & Dropout Correction
**Responsibility:** Extract instantaneous frequency (video) and detect dropouts.
**User Parameters:**
-   `Spike Threshold` (MHz): Frequency level that triggers dropout correction.
    -   *Default:* ~9.6 MHz (2x White Level).
-   `Use Diff Demod` (bool): Enable/Disable the spike replacement algorithm.

### Stage 4: Video Filtering
**Responsibility:** Post-demodulation cleanup.
**User Parameters:**
-   `De-emphasis` (bool): Enable standard VHS de-emphasis.
-   `LPF Cutoff` (MHz): Video bandwidth limit.
    -   *Default:* 3.4 MHz (PAL), 4.2 MHz (NTSC).
-   `Notch Filter` (MHz): Optional notch for removing specific interference.

### Stage 5: Time Base Correction (TBC)
**Responsibility:** Detect sync pulses and resample to a stable grid.
**User Parameters:**
-   `Sync Threshold` (Digital Unit): Level to detect sync tips.
-   `H-Sync Window` (us): Expected width of H-Sync.
-   `V-Sync Integration` (us): For detecting V-Sync.

## Proposed Implementation Plan

1.  **Refactor `RFProcessor` (Immediate):**
    -   Combine Bandpass and Hilbert into a single `ComplexFilter`.
    -   Expose `setBandpass(low, high)` to regenerate this filter on the fly.
    -   This yields a **2x speedup** in the RF stage (Hotspot #1).

2.  **Refactor `FMDemodulator`:**
    -   Expose `setSpikeThreshold(mhz)`.

3.  **Create `Pipeline` Class:**
    -   Orchestrate the flow.
    -   Manage the `FFTEngine` resource (shared between stages).

## Optimization Note: Fused RF Processing
Currently, the code does:
1.  FFT(RF) -> Bandpass -> IFFT -> RF_Filtered
2.  FFT(RF_Filtered) -> Hilbert -> IFFT -> Analytic

New "Fused" approach:
1.  Generate `CombinedFilter = Bandpass * Hilbert` (Pre-calculated).
2.  FFT(RF) -> Multiply(CombinedFilter) -> IFFT -> Analytic
**Result:** Saves 2 FFT operations per block.
