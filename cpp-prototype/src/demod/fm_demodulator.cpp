#define _USE_MATH_DEFINES
#include "vhsdecode/fm_demodulator.hpp"
#include <cmath>
#include <algorithm>

namespace vhsdecode {
namespace demod {

class FMDemodulator::Impl {
public:
    ProcessingConfig config;
    
    explicit Impl(const ProcessingConfig& cfg) : config(cfg) {}
    
    // Phase unwrapping helper
    RealArray unwrapPhase(const ComplexArray& analyticSignal) {
        RealArray phase(analyticSignal.size());
        
        for (size_t i = 0; i < analyticSignal.size(); ++i) {
            phase[i] = std::arg(analyticSignal[i]);
        }
        
        // Simple phase unwrapping
        for (size_t i = 1; i < phase.size(); ++i) {
            float diff = phase[i] - phase[i-1];
            
            // Unwrap if jump > π
            while (diff > M_PI) {
                phase[i] -= 2.0f * M_PI;
                diff = phase[i] - phase[i-1];
            }
            while (diff < -M_PI) {
                phase[i] += 2.0f * M_PI;
                diff = phase[i] - phase[i-1];
            }
        }
        
        return phase;
    }
    
    // Envelope detection
    RealArray detectEnvelope(const ComplexArray& analyticSignal) {
        RealArray envelope(analyticSignal.size());
        
        for (size_t i = 0; i < analyticSignal.size(); ++i) {
            envelope[i] = std::abs(analyticSignal[i]);
        }
        
        return envelope;
    }
    
    // Differentiate phase to get frequency (FM demod)
    RealArray phaseToFrequency(const RealArray& phase, float sampleRate) {
        RealArray freq(phase.size());
        
        freq[0] = 0.0f;
        for (size_t i = 1; i < phase.size(); ++i) {
            freq[i] = (phase[i] - phase[i-1]) * sampleRate / (2.0f * M_PI);
        }
        
        return freq;
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
    
    // 1. Unwrap phase
    auto phase = impl_->unwrapPhase(analyticSignal);
    
    // 2. Differentiate phase to get frequency (FM demodulation)
    float sampleRate = config_.inputFreqMHz * 1e6f;  // Convert MHz to Hz
    result.video = impl_->phaseToFrequency(phase, sampleRate);
    
    // 3. Detect envelope for dropout detection
    result.envelope = impl_->detectEnvelope(analyticSignal);
    
    // 4. For now, copy video to video05 and chroma
    // TODO: Implement proper filtering
    result.video05 = result.video;
    result.chroma = result.video;
    
    return result;
}

} // namespace demod
} // namespace vhsdecode
