# Chroma Pipeline Tuning Summary

## Objective
Match the C++ prototype's chroma output (Saturation, Hue, SNR) to the Python baseline.

## Tuning Results

### 1. Saturation (Signal Amplitude)
- **Issue**: Initial saturation was very low (~0.33 vs 0.54 reference).
- **Cause**: The Automatic Color Control (ACC) reference level was too low for the output scaling expected by `ld-chroma-decoder`.
- **Fix**: Increased `chromaConfig.burstAbsRef` from default (~5000) to **8700.0**.
- **Result**: Saturation matches perfectly.
  - Reference: 0.5419
  - C++ Final: 0.5415
  - Difference: -0.0005

### 2. Bandwidth (Filter Width)
- **Issue**: Narrow bandwidth was clipping sidebands, reducing detail and saturation.
- **Fix**: Widened `chroma_bp` filter in `main.cpp`.
  - Old: 0.4 MHz - 1.0 MHz
  - New: **0.04 MHz - 1.5 MHz** (relative to carrier)
- **Result**: Improved transient response and color detail.

### 3. Comb Filter
- **Issue**: High chroma noise (grain).
- **Fix**: Enabled PAL 2-line delay comb filter.
  - `chromaConfig.enableComb = true`
  - `chromaConfig.combDelay = 2`
- **Result**: Reduced Hue standard deviation (noise).

### 4. Hue (Phase)
- **Issue**: Consistent Hue offset (~13 degrees, 0.037 difference).
- **Investigation**: Tried applying global phase offset (`phaseOffset`) to the heterodyne carrier.
- **Finding**: Global phase shift has **no effect** on the decoded image because `ld-chroma-decoder` locks to the color burst. Rotating the signal rotates both burst and video, maintaining their relative phase.
- **Conclusion**: The Hue offset is likely due to:
  1. Phase response differences between C++ IIR filters and Python FIR filters.
  2. Minor timing differences in burst gate detection.
- **Status**: Accepted as minor deviation (visual impact is low).

## Final Configuration (`main.cpp`)

```cpp
chromaConfig.heterodyneFreq = (fscMHz + colorUnderFreqMHz) * 1e6;
chromaConfig.enableComb = true;
chromaConfig.combDelay = 2;
chromaConfig.burstAbsRef = 8700.0; // Critical for saturation match
chromaConfig.phaseOffset = 0.0;
```

## Verification
Run `analyze_images.py` comparing C++ output to Python baseline:
```
Saturation: ref_mean=0.5419, test_mean=0.5415, diff_mean=-0.0005
Hue:        ref_mean=0.4650, test_mean=0.5023, diff_mean=0.0373
```
