# Modular Architecture Refactor Summary

## Overview
To support UI integration and dynamic parameter tuning, the C++ prototype has been refactored to use a modular pipeline architecture.

## Changes

### 1. FMDemodulator Refactor
- **Configurable Spike Threshold:** Added `setSpikeThreshold(float mhz)` and `getSpikeThreshold()`.
- **Toggleable Spike Replacement:** Added `setSpikeReplacement(bool enable)`.
- **Internal State:** The `Impl` class now stores these parameters, allowing `replaceSpikes` to be called without arguments.

### 2. Pipeline Class
- **New Class:** `vhsdecode::Pipeline` (in `include/vhsdecode/pipeline.hpp`).
- **Responsibility:** Orchestrates the flow from RF input to Video/Chroma output.
- **Stages Managed:**
  - `RFProcessor` (Bandpass, Hilbert)
  - `FMDemodulator` (FM Demod, Spike Replacement)
  - `VideoFilter` (De-emphasis, LPF)
- **Configuration:**
  - `setBandpassFrequencies(low, high)`
  - `setSpikeThreshold(mhz)`
  - `setVideoLPF(cutoff, order)`
  - `setDeemphasis(gain, mid, q)`

### 3. Main Application
- Updated `src/core/main.cpp` to use the new `FMDemodulator` API.
- The `Pipeline` class is available for future integration into a UI or replacing the manual wiring in `main.cpp`.

## Next Steps
1. **Integrate Pipeline into Main:** Replace the manual wiring in `main.cpp` with the `Pipeline` class to fully validate it.
2. **Add Sync Detection:** Incorporate `SyncDetector` and `VSyncDetector` into the `Pipeline` (or a `TBCPipeline` subclass).
3. **Expose to UI:** Create bindings (e.g. via Pybind11 or C interface) for the UI to control the `Pipeline`.
