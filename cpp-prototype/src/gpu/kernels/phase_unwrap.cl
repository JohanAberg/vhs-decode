/**
 * @file phase_unwrap.cl
 * @brief OpenCL kernel for FM demodulation phase unwrapping
 * 
 * Unwraps phase discontinuities in FM demodulated signal.
 * This is a critical operation for FM demodulation accuracy.
 */

// Simple phase unwrap kernel for individual work items
__kernel void phase_unwrap_simple(
    __global const double* phase_in,
    __global double* phase_out,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx == 0) {
        phase_out[0] = phase_in[0];
    } else if (idx < N) {
        double delta = phase_in[idx] - phase_in[idx - 1];
        
        // Wrap to [-π, π]
        if (delta > M_PI) delta -= 2.0 * M_PI;
        if (delta < -M_PI) delta += 2.0 * M_PI;
        
        // Note: This is a simplified version that works well for
        // sequential processing. For true parallel unwrapping,
        // a more sophisticated algorithm would be needed.
        phase_out[idx] = phase_out[idx - 1] + delta;
    }
}

/**
 * @brief Calculate phase differences for unwrapping
 * 
 * This kernel computes phase differences that can be accumulated
 * in a second pass for complete unwrapping.
 */
__kernel void phase_diff(
    __global const double* phase_in,
    __global double* phase_diff_out,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx > 0 && idx < N) {
        double delta = phase_in[idx] - phase_in[idx - 1];
        
        // Wrap to [-π, π]
        if (delta > M_PI) delta -= 2.0 * M_PI;
        if (delta < -M_PI) delta += 2.0 * M_PI;
        
        phase_diff_out[idx] = delta;
    } else if (idx == 0) {
        phase_diff_out[0] = phase_in[0];
    }
}

/**
 * @brief Accumulate phase differences (prefix sum)
 * 
 * This would typically use a parallel scan algorithm
 * for maximum performance. For now, we use a simple accumulation.
 */
__kernel void phase_accumulate(
    __global const double* phase_diff,
    __global double* phase_out,
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx == 0) {
        phase_out[0] = phase_diff[0];
    } else if (idx < N) {
        // Sequential accumulation
        // TODO: Replace with parallel prefix sum for better performance
        double sum = phase_diff[0];
        for (int i = 1; i <= idx; ++i) {
            sum += phase_diff[i];
        }
        phase_out[idx] = sum;
    }
}
