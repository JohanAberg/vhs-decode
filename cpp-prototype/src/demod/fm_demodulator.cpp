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
    
    // FM demodulation matching Python's unwrap_hilbert() algorithm exactly
    // Algorithm from lddecode/utils.py:
    // 1. tangles = np.angle(hilbert)
    // 2. dangles = np.ediff1d(tangles, to_begin=0)
    // 3. dangles = np.unwrap(dangles)  - cumulative unwrap for phase jumps
    // 4. Clamp to [0, tau] for bad data
    // 5. return dangles * (freq_hz / tau)
    RealArray fmDemodulate(const ComplexArray& analyticSignal, float sampleRate) {
        const size_t N = analyticSignal.size();
        RealArray dangles(N);
        
        // Step 1 & 2: Get phase angles and compute first difference (ediff1d)
        dangles[0] = 0.0f;  // to_begin=0 in Python ediff1d
        
        for (size_t i = 1; i < N; ++i) {
            float prevAngle = std::arg(analyticSignal[i-1]);
            float currAngle = std::arg(analyticSignal[i]);
            dangles[i] = currAngle - prevAngle;
        }
        
        // Step 3: np.unwrap() on differences
        // np.unwrap corrects for 2π discontinuities cumulatively
        // For each sample, if the jump from previous is > π, subtract 2π
        // if jump < -π, add 2π
        // For phase differences that are already per-sample, this is equivalent
        // to bringing each sample into [-π, π]
        for (size_t i = 0; i < N; ++i) {
            while (dangles[i] > static_cast<float>(M_PI)) {
                dangles[i] -= TAU;
            }
            while (dangles[i] < -static_cast<float>(M_PI)) {
                dangles[i] += TAU;
            }
        }
        
        // Step 4: Clamp to [0, tau] for bad data
        // "With extremely bad data, the unwrapped angles can jump"
        for (size_t i = 0; i < N; ++i) {
            while (dangles[i] < 0.0f) {
                dangles[i] += TAU;
            }
            while (dangles[i] > TAU) {
                dangles[i] -= TAU;
            }
        }
        
        // Step 5: Scale to frequency in Hz
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
