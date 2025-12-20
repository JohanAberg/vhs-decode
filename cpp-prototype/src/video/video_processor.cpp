#include "vhsdecode/video_processor.hpp"
#include "vhsdecode/fm_demodulator.hpp"
#include "vhsdecode/chroma_separator.hpp"
#include "vhsdecode/dropout_corrector.hpp"
#include "vhsdecode/tbc_corrector.hpp"
#include "vhsdecode/filter_bank.hpp"
#include "vhsdecode/fftw_engine.hpp"
#include <iostream>
#include <chrono>

namespace vhsdecode {
namespace video {

class VideoProcessor::Impl {
public:
    Config config;
    std::unique_ptr<demod::FMDemodulator> fmDemodulator;
    std::unique_ptr<rf::FilterBank> filterBank;
    std::unique_ptr<rf::FFTWEngine> fftwEngine;
    std::unique_ptr<ChromaSeparator> chromaSeparator;
    std::unique_ptr<DropoutCorrector> dropoutCorrector;
    std::unique_ptr<TBCCorrector> tbcCorrector;

    Impl(const Config& cfg)
        : config(cfg)
    {
        // Initialize FM demodulator
        demod::FMDemodulator::Config fmConfig;
        fmConfig.sampleRate = config.sampleRate * 1e6;  // Convert to Hz
        fmDemodulator = std::make_unique<demod::FMDemodulator>(fmConfig);

        // Initialize filter bank and FFT engine for chroma separation
        filterBank = std::make_unique<rf::FilterBank>(config.sampleRate, 32768);
        filterBank->createLowpassFilter("video_lpf", 6.0);
        filterBank->createBandpassFilter("chroma_bp", 0.4, 1.0);

        fftwEngine = std::make_unique<rf::FFTWEngine>(32768, true);

        // Initialize chroma separator
        chromaSeparator = std::make_unique<ChromaSeparator>(*filterBank, *fftwEngine);

        // Initialize dropout corrector
        dropoutCorrector = std::make_unique<DropoutCorrector>(config.dropoutThreshold);

        // Initialize TBC corrector
        tbcCorrector = std::make_unique<TBCCorrector>(config.system, config.sampleRate);
    }
};

VideoProcessor::VideoProcessor(const Config& config)
    : pImpl_(std::make_unique<Impl>(config))
{
    std::cout << "✓ Video processor initialized" << std::endl;
}

VideoProcessor::~VideoProcessor() = default;

VideoProcessor::ProcessingResult
VideoProcessor::processAnalyticSignal(
    const std::vector<std::complex<double>>& analyticSignal,
    const std::vector<double>& envelope
) {
    auto startTime = std::chrono::high_resolution_clock::now();

    ProcessingResult result;

    // 1. FM demodulation
    auto demodResult = pImpl_->fmDemodulator->demodulate(analyticSignal);

    // 2. Dropout detection and correction
    auto dropoutResult = pImpl_->dropoutCorrector->detectAndCorrect(
        demodResult.video, envelope
    );
    result.dropoutCount = dropoutResult.dropouts.size();

    // 3. Video/chroma separation
    auto sepResult = pImpl_->chromaSeparator->separate(dropoutResult.correctedSignal);
    result.video = sepResult.video;
    result.chroma = sepResult.chroma;

    // 4. Time-base correction
    auto tbcResult = pImpl_->tbcCorrector->correctAndAssemble(result.video);
    result.fields = tbcResult.fields;
    result.totalLines = tbcResult.totalLines;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    result.processingTimeMs = duration.count() / 1000.0;

    return result;
}

} // namespace video
} // namespace vhsdecode
