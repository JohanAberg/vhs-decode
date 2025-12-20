/**
 * @file filter_apply.cl
 * @brief OpenCL kernels for filter application
 * 
 * Applies frequency-domain filters to FFT data.
 * This is a key operation in the RF processing pipeline.
 */

/**
 * @brief Apply frequency-domain filter (complex input/output)
 * 
 * Multiplies complex FFT data by real filter coefficients.
 * 
 * @param fft_data Complex FFT data (in/out)
 * @param filter_response Real filter response
 * @param N Number of frequency bins
 */
__kernel void apply_filter_freq_domain(
    __global double2* fft_data,           // Complex FFT data (in/out)
    __global const double* filter_response,  // Real filter response
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 data = fft_data[idx];
        double gain = filter_response[idx];
        
        fft_data[idx].x = data.x * gain;
        fft_data[idx].y = data.y * gain;
    }
}

/**
 * @brief Apply frequency-domain filter with complex coefficients
 * 
 * Multiplies complex FFT data by complex filter coefficients.
 * Used for filters that have phase response.
 * 
 * @param fft_data Complex FFT data (in/out)
 * @param filter_response Complex filter response
 * @param N Number of frequency bins
 */
__kernel void apply_complex_filter_freq_domain(
    __global double2* fft_data,
    __global const double2* filter_response,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 data = fft_data[idx];
        double2 filter = filter_response[idx];
        
        // Complex multiplication: (a + bi)(c + di) = (ac - bd) + (ad + bc)i
        double real = data.x * filter.x - data.y * filter.y;
        double imag = data.x * filter.y + data.y * filter.x;
        
        fft_data[idx].x = real;
        fft_data[idx].y = imag;
    }
}

/**
 * @brief Apply multiple filters in sequence
 * 
 * Applies multiple filters to the same FFT data efficiently.
 * Useful for cascaded filtering operations.
 * 
 * @param fft_data Complex FFT data (in/out)
 * @param filters Array of filter responses
 * @param num_filters Number of filters to apply
 * @param N Number of frequency bins
 */
__kernel void apply_multiple_filters(
    __global double2* fft_data,
    __global const double* filters,  // filters[num_filters * N]
    const int num_filters,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 data = fft_data[idx];
        
        // Apply each filter in sequence
        for (int f = 0; f < num_filters; ++f) {
            double gain = filters[f * N + idx];
            data.x *= gain;
            data.y *= gain;
        }
        
        fft_data[idx] = data;
    }
}

/**
 * @brief Bandpass filter application
 * 
 * Applies a bandpass filter with specified low and high cutoff frequencies.
 * This is computed on-the-fly rather than using precomputed coefficients.
 * 
 * @param fft_data Complex FFT data (in/out)
 * @param N Number of frequency bins
 * @param low_cutoff Low cutoff frequency (normalized, 0-1)
 * @param high_cutoff High cutoff frequency (normalized, 0-1)
 * @param rolloff Rolloff factor (steepness of transition)
 */
__kernel void apply_bandpass_filter(
    __global double2* fft_data,
    const int N,
    const double low_cutoff,
    const double high_cutoff,
    const double rolloff
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double freq = (double)idx / N;  // Normalized frequency
        
        double gain = 1.0;
        
        // Low-frequency rolloff
        if (freq < low_cutoff) {
            double t = (freq - low_cutoff) / rolloff;
            gain *= 0.5 * (1.0 + tanh(t));
        }
        
        // High-frequency rolloff
        if (freq > high_cutoff) {
            double t = (high_cutoff - freq) / rolloff;
            gain *= 0.5 * (1.0 + tanh(t));
        }
        
        double2 data = fft_data[idx];
        fft_data[idx].x = data.x * gain;
        fft_data[idx].y = data.y * gain;
    }
}

/**
 * @brief Normalize FFT data
 * 
 * Scales FFT data by a normalization factor.
 * Typically used after inverse FFT.
 * 
 * @param data Real or complex data (in/out)
 * @param N Number of samples
 * @param scale Scale factor (typically 1/N for FFT normalization)
 */
__kernel void normalize_data(
    __global double* data,
    const int N,
    const double scale
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        data[idx] *= scale;
    }
}

/**
 * @brief Normalize complex FFT data
 */
__kernel void normalize_complex_data(
    __global double2* data,
    const int N,
    const double scale
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        data[idx].x *= scale;
        data[idx].y *= scale;
    }
}
