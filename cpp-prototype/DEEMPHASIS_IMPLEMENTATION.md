# VHS De-emphasis Implementation Details

**Date:** December 22, 2025
**Component:** Signal Processing / Filter Bank

## Overview

VHS recording applies "pre-emphasis" to the video signal, boosting high frequencies to improve the signal-to-noise ratio. During playback (decoding), a complementary "de-emphasis" filter must be applied to attenuate these high frequencies back to their original levels. Without this, the decoded video appears extremely sharp and noisy.

## Python Reference

The Python implementation uses a digital IIR filter designed to mimic an analog shelving filter.
- **Source:** `vhsdecode/addons/FMdeemph.py`
- **Function:** `gen_shelf`
- **Logic:** Generates a high-shelf filter and then inverts it (swaps numerator `b` and denominator `a` coefficients) to create a low-shelf (de-emphasis) response.

### Parameters (PAL)
- **Gain:** 13.9794 dB
- **Mid Frequency:** 273,755.82 Hz
- **Q Factor:** 0.462088186

## C++ Port

The implementation was ported to `cpp-prototype/src/rf/filter_bank.cpp`.

### 1. Filter Coefficient Generation
We implemented the `gen_shelf` algorithm exactly as it appears in the Python codebase. This calculates the coefficients for a standard high-shelf filter based on the gain, center frequency, and Q-factor.

### 2. Inversion for De-emphasis
To turn the high-shelf boost (pre-emphasis) into a cut (de-emphasis), we invert the transfer function:
$$ H_{deemph}(z) = \frac{1}{H_{shelf}(z)} $$

In terms of IIR coefficients:
- $b_{new} = a_{old}$
- $a_{new} = b_{old}$

### 3. Frequency Domain Application
Since the C++ prototype processes data in blocks using FFTs, we apply the IIR filter in the frequency domain rather than the time domain.

1.  **Compute Frequency Response:** We evaluate the filter's transfer function $H(z)$ at each FFT bin frequency $z = e^{j\omega}$.
    $$ H(e^{j\omega}) = \frac{b_0 + b_1 e^{-j\omega} + b_2 e^{-2j\omega}}{a_0 + a_1 e^{-j\omega} + a_2 e^{-2j\omega}} $$
2.  **Store Response:** The resulting complex values are stored as a `ComplexArray` in the `FilterBank`.
3.  **Apply:** The `FFTEngine` multiplies the signal's FFT by this response array.

## Results

-   **Visual Quality:** Drastic reduction in high-frequency noise ("sparkles" and "grain").
-   **Metrics:**
    -   Gradient Ratio (C++ vs Python) improved from **10.31** (10x noisier) to **2.31**.
    -   Mean Absolute Difference improved from **25.04** to **11.53**.

## Code Location

-   **Header:** `cpp-prototype/include/vhsdecode/filter_bank.hpp`
-   **Implementation:** `cpp-prototype/src/rf/filter_bank.cpp` (`createDeemphasisFilter`)
-   **Usage:** `cpp-prototype/src/core/main.cpp` (Main decode loop)
