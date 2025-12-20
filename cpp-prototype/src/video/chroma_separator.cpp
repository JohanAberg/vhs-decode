#include "vhsdecode/chroma_separator.hpp"
#include "vhsdecode/filter_bank.hpp"
#include "vhsdecode/fftw_engine.hpp"
#include <iostream>

namespace vhsdecode {
namespace video {

class ChromaSeparator::Impl {
public:
    FilterBank& filterBank;
    FFTWEngine& fftEngine;

    Impl(FilterBank& fb, FFTWEngine& fft)
        : filterBank(fb), fftEngine(fft) {}
};

ChromaSeparator::ChromaSeparator(FilterBank& filterBank, FFTWEngine& fftEngine)
    : pImpl(std::make_unique<Impl>(filterBank, fftEngine)) {
    std::cout << "✓ Chroma separator initialized" << std::endl;
    std::cout << "  Video LPF: 0-6.0 MHz" << std::endl;
    std::cout << "  Chroma BPF: 0.4-1.0 MHz" << std::endl;
}

ChromaSeparator::~ChromaSeparator() = default;

ChromaSeparator::SeparationResult 
ChromaSeparator::separate(const std::vector<double>& demodulated) {
    SeparationResult result;
    
    // Extract video (luminance) using lowpass filter
    auto videoFFT = pImpl->fftEngine.forwardFFT(demodulated);
    auto videoFilter = pImpl->filterBank.getFilter("video_lpf");
    pImpl->fftEngine.applyFilter(videoFFT, videoFilter);
    result.video = pImpl->fftEngine.inverseFFT(videoFFT);
    
    // Extract chroma using bandpass filter
    auto chromaFFT = pImpl->fftEngine.forwardFFT(demodulated);
    auto chromaFilter = pImpl->filterBank.getFilter("chroma_bp");
    pImpl->fftEngine.applyFilter(chromaFFT, chromaFilter);
    result.chroma = pImpl->fftEngine.inverseFFT(chromaFFT);
    
    return result;
}

} // namespace video
} // namespace vhsdecode
