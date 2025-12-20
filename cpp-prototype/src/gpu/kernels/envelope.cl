/**
 * @file envelope.cl
 * @brief OpenCL kernel for envelope detection
 * 
 * Computes the magnitude (envelope) of complex analytic signal.
 * Used for dropout detection and signal quality assessment.
 */

/**
 * @brief Detect envelope from complex analytic signal
 * 
 * @param analytic_signal Complex input (real and imaginary parts)
 * @param envelope Real output (magnitude)
 * @param N Number of samples
 */
__kernel void envelope_detect(
    __global const double2* analytic_signal,  // Complex input
    __global double* envelope,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 z = analytic_signal[idx];
        envelope[idx] = sqrt(z.x * z.x + z.y * z.y);  // Magnitude
    }
}

/**
 * @brief Fast envelope detection using built-in functions
 * 
 * Uses OpenCL built-in length() function which may be optimized
 * on some hardware.
 */
__kernel void envelope_detect_fast(
    __global const double2* analytic_signal,
    __global double* envelope,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 z = analytic_signal[idx];
        envelope[idx] = length(z);  // Built-in magnitude function
    }
}

/**
 * @brief Envelope detection with lowpass filtering
 * 
 * Applies a simple moving average filter to smooth the envelope.
 * Useful for dropout detection.
 * 
 * @param analytic_signal Complex input
 * @param envelope Smoothed envelope output
 * @param N Number of samples
 * @param window_size Filter window size (must be odd)
 */
__kernel void envelope_detect_filtered(
    __global const double2* analytic_signal,
    __global double* envelope,
    const int N,
    const int window_size
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 z = analytic_signal[idx];
        double mag = sqrt(z.x * z.x + z.y * z.y);
        
        // Apply moving average filter
        int half_window = window_size / 2;
        double sum = 0.0;
        int count = 0;
        
        for (int i = -half_window; i <= half_window; ++i) {
            int sample_idx = idx + i;
            if (sample_idx >= 0 && sample_idx < N) {
                double2 sample = analytic_signal[sample_idx];
                sum += sqrt(sample.x * sample.x + sample.y * sample.y);
                count++;
            }
        }
        
        envelope[idx] = sum / count;
    }
}

/**
 * @brief Compute instantaneous frequency from complex signal
 * 
 * This is related to envelope detection and useful for FM demodulation.
 * 
 * @param analytic_signal Complex input
 * @param frequency Instantaneous frequency output
 * @param N Number of samples
 * @param sample_rate Sample rate in Hz
 */
__kernel void instantaneous_frequency(
    __global const double2* analytic_signal,
    __global double* frequency,
    const int N,
    const double sample_rate
) {
    int idx = get_global_id(0);
    
    if (idx > 0 && idx < N) {
        double2 z1 = analytic_signal[idx - 1];
        double2 z2 = analytic_signal[idx];
        
        // Compute phase difference
        double phase1 = atan2(z1.y, z1.x);
        double phase2 = atan2(z2.y, z2.x);
        double delta = phase2 - phase1;
        
        // Unwrap
        if (delta > M_PI) delta -= 2.0 * M_PI;
        if (delta < -M_PI) delta += 2.0 * M_PI;
        
        // Convert to frequency
        frequency[idx] = delta * sample_rate / (2.0 * M_PI);
    } else if (idx == 0) {
        frequency[0] = 0.0;
    }
}
