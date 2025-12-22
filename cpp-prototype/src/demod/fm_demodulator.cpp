#define _USE_MATH_DEFINES
#include "vhsdecode/fm_demodulator.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>

namespace vhsdecode {
namespace demod {

class FMDemodulator::Impl {
public:
    ProcessingConfig config;
    static constexpr float TAU = 2.0f * static_cast<float>(M_PI);
    bool debugPrinted = false;
    
    // State for block continuity
    std::complex<float> lastSample_ = {0.0f, 0.0f};
    bool firstBlock_ = true;
    
    // Configuration
    float spikeThresholdHz_ = 0.0f;
    bool spikeReplacementEnabled_ = false;

    explicit Impl(const ProcessingConfig& cfg) : config(cfg) {
        // Default threshold: 10 MHz (safe high value)
        spikeThresholdHz_ = 10.0e6f;
    }
    
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
        
        // Handle block boundary for first sample
        if (firstBlock_) {
            dangles[0] = 0.0f;  // to_begin=0 in Python ediff1d
            firstBlock_ = false;
        } else {
            float prevAngle = std::arg(lastSample_);
            float currAngle = std::arg(analyticSignal[0]);
            dangles[0] = currAngle - prevAngle;
        }
        
        for (size_t i = 1; i < N; ++i) {
            float prevAngle = std::arg(analyticSignal[i-1]);
            float currAngle = std::arg(analyticSignal[i]);
            dangles[i] = currAngle - prevAngle;
        }
        
        // Store last sample for next block
        if (N > 0) {
            lastSample_ = analyticSignal[N-1];
        }
        
        // Debug: print phase differences before unwrap
        if (!debugPrinted) {
            debugPrinted = true;
            std::cout << "\n=== FM Demod Debug (first block) ===\n";
            std::cout << "dangles BEFORE unwrap (first 20): [";
            for (size_t i = 0; i < 20 && i < N; ++i) {
                std::cout << std::fixed << std::setprecision(4) << dangles[i];
                if (i < 19) std::cout << ", ";
            }
            std::cout << "]\n";
            std::cout << "dangles min: " << *std::min_element(dangles.begin(), dangles.end()) << ", ";
            std::cout << "max: " << *std::max_element(dangles.begin(), dangles.end()) << "\n";
        }
        
        // Make sure unwrapping goes the right way (Python check)
        if (dangles[0] < -static_cast<float>(M_PI)) {
            dangles[0] += TAU;
        }
        
        // Step 3: np.unwrap() - adjust consecutive values to differ by < π
        // numpy.unwrap makes phase differences continuous by adding/subtracting 2π
        // to each element so that consecutive values differ by less than π
        for (size_t i = 1; i < N; ++i) {
            float diff = dangles[i] - dangles[i-1];
            // Wrap diff to [-π, π]
            float wrappedDiff = std::fmod(diff + static_cast<float>(M_PI), TAU);
            if (wrappedDiff < 0) {
                wrappedDiff += TAU;
            }
            wrappedDiff -= static_cast<float>(M_PI);
            // Adjust dangles[i] by the difference
            dangles[i] = dangles[i-1] + wrappedDiff;
        }
        
        // Debug: print after unwrap, before clamp
        static bool debugAfterUnwrap = false;
        if (!debugAfterUnwrap) {
            debugAfterUnwrap = true;
            std::cout << "dangles AFTER unwrap (first 20): [";
            for (size_t i = 0; i < 20 && i < N; ++i) {
                std::cout << std::fixed << std::setprecision(4) << dangles[i];
                if (i < 19) std::cout << ", ";
            }
            std::cout << "]\n";
            std::cout << "After unwrap min: " << *std::min_element(dangles.begin(), dangles.end()) << ", ";
            std::cout << "max: " << *std::max_element(dangles.begin(), dangles.end()) << "\n";
        }
        
        // Step 4: Clamp to [0, tau] for bad data
        // "With extremely bad data, the unwrapped angles can jump"
        float minVal = *std::min_element(dangles.begin(), dangles.end());
        float maxVal = *std::max_element(dangles.begin(), dangles.end());
        
        // Python: while np.min(dangles) < 0: dangles[dangles < 0] += tau
        while (minVal < 0.0f) {
            for (size_t i = 0; i < N; ++i) {
                if (dangles[i] < 0.0f) {
                    dangles[i] += TAU;
                }
            }
            minVal = *std::min_element(dangles.begin(), dangles.end());
        }
        
        // Python: while np.max(dangles) > tau: dangles[dangles > tau] -= tau
        while (maxVal > TAU) {
            for (size_t i = 0; i < N; ++i) {
                if (dangles[i] > TAU) {
                    dangles[i] -= TAU;
                }
            }
            maxVal = *std::max_element(dangles.begin(), dangles.end());
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

    void reset() {
        firstBlock_ = true;
        lastSample_ = {0.0f, 0.0f};
    }

    size_t replaceSpikes(RealArray& video, const ComplexArray& analyticSignal, float threshold) {
        const size_t N = video.size();
        std::vector<size_t> toFix;
        
        // Find spikes
        // Python: if np.max(demod[20:-20]) > check_value:
        size_t margin = 20;
        if (N <= 2 * margin) return 0;

        for (size_t i = margin; i < N - margin; ++i) {
            if (video[i] > threshold) {
                toFix.push_back(i);
            }
        }
        
        if (toFix.empty()) return 0;
        
        // Compute diff of analytic signal
        // Python: np.ediff1d(hilbert, to_begin=0)
        ComplexArray diffSignal(N);
        diffSignal[0] = std::complex<float>(0, 0);
        for (size_t i = 1; i < N; ++i) {
            diffSignal[i] = analyticSignal[i] - analyticSignal[i-1];
        }
        
        // Demodulate diff signal
        RealArray demodDiffed = fmDemodulate(diffSignal, config.inputFreqMHz * 1e6f);
        
        // Apply replacement
        // Python: replace_spikes(demod, demod_b, check_value, replace_start=8, replace_end=30)
        int replaceStart = 8;
        int replaceEnd = 30;
        
        size_t replacedCount = 0;
        for (size_t i : toFix) {
            size_t start = (i >= static_cast<size_t>(replaceStart)) ? i - replaceStart : 0;
            size_t end = std::min(i + replaceEnd, N - 1);
            
            // Only replace if it seems to help
            // if max(demod_diffed[start:end]) < max(demod[start:end]):
            float maxDiffed = -std::numeric_limits<float>::infinity();
            float maxOriginal = -std::numeric_limits<float>::infinity();
            
            for (size_t j = start; j < end; ++j) {
                maxDiffed = std::max(maxDiffed, demodDiffed[j]);
                maxOriginal = std::max(maxOriginal, video[j]);
            }
            
            if (maxDiffed < maxOriginal) {
                for (size_t j = start; j < end; ++j) {
                    video[j] = demodDiffed[j];
                }
                replacedCount++;
            }
        }
        return replacedCount;
    }
};

FMDemodulator::FMDemodulator(const ProcessingConfig& config)
    : impl_(std::make_unique<Impl>(config))
    , config_(config)
{
}

FMDemodulator::~FMDemodulator() = default;

void FMDemodulator::reset() {
    impl_->reset();
}

void FMDemodulator::setSpikeThreshold(float thresholdMHz) {
    impl_->spikeThresholdHz_ = thresholdMHz * 1e6f;
}

float FMDemodulator::getSpikeThreshold() const {
    return impl_->spikeThresholdHz_ / 1e6f;
}

void FMDemodulator::setSpikeReplacement(bool enable) {
    impl_->spikeReplacementEnabled_ = enable;
}

bool FMDemodulator::isSpikeReplacementEnabled() const {
    return impl_->spikeReplacementEnabled_;
}

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

size_t FMDemodulator::replaceSpikes(RealArray& video, const ComplexArray& analyticSignal) {
    if (!impl_->spikeReplacementEnabled_) {
        return 0;
    }
    return impl_->replaceSpikes(video, analyticSignal, impl_->spikeThresholdHz_);
}

size_t FMDemodulator::replaceSpikes(RealArray& video, const ComplexArray& analyticSignal, float threshold) {
    return impl_->replaceSpikes(video, analyticSignal, threshold);
}

} // namespace demod
} // namespace vhsdecode
