#define _USE_MATH_DEFINES
#include "vhsdecode/fm_demodulator.hpp"
#include <cmath>
#include <algorithm>

namespace vhsdecode {
namespace demod {

class FMDemodulator::Impl {
public:
    ProcessingConfig config;
    static constexpr float TAU = 2.0f * static_cast<float>(M_PI);
    
    explicit Impl(const ProcessingConfig& cfg) : config(cfg) {}
    
    // FM demodulation matching Python's unwrap_hilbert()
    // Algorithm:
    // 1. Get phase angles from analytic signal
    // 2. Compute first difference of angles (ediff1d)
    // 3. Unwrap the phase differences
    // 4. Clamp to [0, tau] range for bad data
    // 5. Scale by (sampleRate / tau) to get frequency in Hz
    RealArray fmDemodulate(const ComplexArray& analyticSignal, float sampleRate) {
        const size_t N = analyticSignal.size();
        RealArray dangles(N);
        RealArray envelope(N);
        
        // First pass: compute envelope for gating
        float maxEnv = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            envelope[i] = std::abs(analyticSignal[i]);
            if (envelope[i] > maxEnv) maxEnv = envelope[i];
        }
        
        // Envelope threshold for valid signal (10% of max)
        float envThreshold = maxEnv * 0.1f;
        
        // Expected frequency for blanking (when no signal)
        // Use approximate video carrier center frequency
        float blankingFreq = 4.0e6f;  // 4 MHz - approximate video carrier
        
        // Step 1: Get phase angles and compute first difference
        float prevAngle = std::arg(analyticSignal[0]);
        dangles[0] = 0.0f;  // to_begin=0 in Python ediff1d
        
        for (size_t i = 1; i < N; ++i) {
            // Gate: if envelope is too low, skip phase tracking
            if (envelope[i] < envThreshold || envelope[i-1] < envThreshold) {
                dangles[i] = blankingFreq * TAU / sampleRate;  // Use blanking frequency
                prevAngle = std::arg(analyticSignal[i]);  // Reset phase tracking
                continue;
            }
            
            float currAngle = std::arg(analyticSignal[i]);
            dangles[i] = currAngle - prevAngle;
            prevAngle = currAngle;
            
            // Normalize to [-pi, pi] range (unwrap)
            while (dangles[i] > static_cast<float>(M_PI)) {
                dangles[i] -= TAU;
            }
            while (dangles[i] < -static_cast<float>(M_PI)) {
                dangles[i] += TAU;
            }
        }
        
        // Step 3: Clamp to [0, tau] range to handle bad data
        for (size_t i = 0; i < N; ++i) {
            while (dangles[i] < 0.0f) {
                dangles[i] += TAU;
            }
            while (dangles[i] > TAU) {
                dangles[i] -= TAU;
            }
        }
        
        // Step 4: Scale to frequency in Hz
        // Formula: freq = dangles * (sampleRate / tau)
        float scale = sampleRate / TAU;
        for (size_t i = 0; i < N; ++i) {
            dangles[i] *= scale;
        }
        
        return dangles;
    }
    
    // Envelope detection
    RealArray detectEnvelope(const ComplexArray& analyticSignal) {
        RealArray envelope(analyticSignal.size());
        
        for (size_t i = 0; i < analyticSignal.size(); ++i) {
            envelope[i] = std::abs(analyticSignal[i]);
        }
        
        return envelope;
    }
};

FMDemodulator::FMDemodulator(const ProcessingConfig& config)
    : impl_(std::make_unique<Impl>(config))
    , config_(config)
{
}

FMDemodulator::~FMDemodulator() = default;

FMDemodulator::FMDemodulator(FMDemodulator&&) noexcept = default;
FMDemodulator& FMDemodulator::operator=(FMDemodulator&&) noexcept = default;

FMDemodulator::Result FMDemodulator::demodulate(const ComplexArray& analyticSignal) {
    Result result;
    
    // FM demodulation using Python-compatible algorithm
    float sampleRate = config_.inputFreqMHz * 1e6f;  // Convert MHz to Hz
    result.video = impl_->fmDemodulate(analyticSignal, sampleRate);
    
    // Detect envelope for dropout detection
    result.envelope = impl_->detectEnvelope(analyticSignal);
    
    // For now, copy video to video05 and chroma
    // TODO: Implement proper filtering
    result.video05 = result.video;
    result.chroma = result.video;
    
    return result;
}

} // namespace demod
} // namespace vhsdecode
