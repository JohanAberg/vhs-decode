#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <memory>

namespace vhsdecode {
namespace filter {

/**
 * @brief Second-order section (SOS) IIR filter
 * 
 * Applies cascaded biquad sections for IIR filtering.
 * SOS coefficients should be generated from scipy.signal.butter(..., output='sos')
 */
class SOSFilter {
public:
    /**
     * @brief Initialize filter with precomputed SOS coefficients
     * @param sos Array of SOS sections, each with 6 coefficients [b0, b1, b2, a0, a1, a2]
     */
    SOSFilter(const std::vector<std::array<double, 6>>& sos) 
        : sos_(sos), states_(sos.size(), {{0.0, 0.0}}) 
    {}
    
    /**
     * @brief Reset filter states
     */
    void reset() {
        for (auto& s : states_) {
            s[0] = 0.0;
            s[1] = 0.0;
        }
    }
    
    /**
     * @brief Apply filter (single pass, forward only)
     */
    std::vector<double> filter(const std::vector<double>& input) {
        reset();
        std::vector<double> output(input.size());
        
        for (size_t i = 0; i < input.size(); ++i) {
            double x = input[i];
            
            // Process through each biquad section (Direct Form II Transposed)
            for (size_t s = 0; s < sos_.size(); ++s) {
                const auto& c = sos_[s];
                double b0 = c[0], b1 = c[1], b2 = c[2];
                double a0 = c[3], a1 = c[4], a2 = c[5];
                
                // Direct Form II Transposed
                double y = (b0/a0) * x + states_[s][0];
                states_[s][0] = (b1/a0) * x - (a1/a0) * y + states_[s][1];
                states_[s][1] = (b2/a0) * x - (a2/a0) * y;
                
                x = y;  // Output becomes input to next section
            }
            
            output[i] = x;
        }
        
        return output;
    }
    
    /**
     * @brief Apply zero-phase filter (forward-backward, matches scipy.signal.filtfilt)
     */
    std::vector<double> filtfilt(const std::vector<double>& input) {
        if (input.empty()) return {};
        
        // Forward pass
        std::vector<double> forward = filter(input);
        
        // Reverse
        std::reverse(forward.begin(), forward.end());
        
        // Backward pass (reset states)
        std::vector<double> backward = filter(forward);
        
        // Reverse to get final result
        std::reverse(backward.begin(), backward.end());
        
        return backward;
    }
    
private:
    std::vector<std::array<double, 6>> sos_;
    std::vector<std::array<double, 2>> states_;
};

/**
 * @brief Butterworth low-pass filter with precomputed coefficients for VHS
 * 
 * Coefficients generated from scipy.signal.butter(order, cutoff/nyquist, 'lowpass', output='sos')
 */
class ButterworthLPF {
public:
    /**
     * @brief Create VHS PAL or NTSC video low-pass filter
     * @param systemPAL true for PAL (3.4 MHz), false for NTSC (6.6 MHz)
     * @param sampleRateHz Sample rate in Hz (typically 40 MHz)
     */
    ButterworthLPF(bool systemPAL, double sampleRateHz)
        : sampleRateHz_(sampleRateHz), isPAL_(systemPAL)
    {
        // Precomputed coefficients from scipy.signal.butter @ 40 MHz
        if (sampleRateHz == 40e6) {
            if (systemPAL) {
                // PAL: 3.4 MHz, 6th order Butterworth @ 40 MHz
                // scipy.signal.butter(6, 3.4e6/(40e6/2), 'lowpass', output='sos')
                sos_ = {
                    {{1.470339420804142e-04, 2.940678841608284e-04, 1.470339420804142e-04, 
                      1.000000000000000e+00, -1.154044634666572e+00, 3.407555323905251e-01}},
                    {{1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00, 
                      1.000000000000000e+00, -1.265846767214598e+00, 4.706459397837652e-01}},
                    {{1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00, 
                      1.000000000000000e+00, -1.521082078595481e+00, 7.671753334620334e-01}}
                };
            } else {
                // NTSC: 6.6 MHz, 9th order Butterworth @ 40 MHz
                // scipy.signal.butter(9, 6.6e6/(40e6/2), 'lowpass', output='sos')
                sos_ = {
                    {{9.102091403206655e-04, 1.820418280641331e-03, 9.102091403206655e-04,
                      1.000000000000000e+00, -8.149757892892195e-01, 1.803395166795548e-01}},
                    {{1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00,
                      1.000000000000000e+00, -7.788620197296645e-01, 2.500200538614155e-01}},
                    {{1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00,
                      1.000000000000000e+00, -7.224915139684889e-01, 3.581959671251161e-01}},
                    {{1.000000000000000e+00, 2.000000000000000e+00, 1.000000000000000e+00,
                      1.000000000000000e+00, -6.540135313287178e-01, 5.055610161631091e-01}},
                    {{1.000000000000000e+00, 1.000000000000000e+00, 0.000000000000000e+00,
                      1.000000000000000e+00, -6.039407016779971e-01, 0.000000000000000e+00}}
                };
            }
        } else {
            throw std::invalid_argument("Only 40 MHz sample rate supported with precomputed coefficients");
        }
        
        filter_ = std::make_unique<SOSFilter>(sos_);
    }
    
    /**
     * @brief Compatibility constructor for existing code
     */
    ButterworthLPF(int /*order*/, double /*cutoffHz*/, double sampleRateHz)
        : ButterworthLPF(true, sampleRateHz)  // Default to PAL
    {}
    
    /**
     * @brief Apply filter to float array in-place
     */
    void filterInPlace(std::vector<float>& data) {
        // Convert to double for precision
        std::vector<double> ddata(data.begin(), data.end());
        
        // Apply zero-phase filter
        auto filtered = filter_->filtfilt(ddata);
        
        // Convert back to float
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<float>(filtered[i]);
        }
    }
    
    /**
     * @brief Apply filter and return result
     */
    std::vector<float> filter(const std::vector<float>& data) {
        std::vector<float> result = data;
        filterInPlace(result);
        return result;
    }
    
private:
    double sampleRateHz_;
    bool isPAL_;
    std::vector<std::array<double, 6>> sos_;
    std::unique_ptr<SOSFilter> filter_;
};

} // namespace filter
} // namespace vhsdecode
