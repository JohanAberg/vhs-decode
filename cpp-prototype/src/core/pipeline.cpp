#include "vhsdecode/pipeline.hpp"
#include "vhsdecode/fft_engine.hpp"
#include "vhsdecode/filter_bank.hpp"
#include "formats/format_base.hpp"
#include <algorithm>
#include <iostream>

namespace vhsdecode {

class Pipeline::Impl {
public:
    Config config;
    
    // Stages
    std::unique_ptr<rf::RFProcessor> rfProcessor;
    std::unique_ptr<demod::FMDemodulator> fmDemod;
    
    // Video filtering
    std::unique_ptr<rf::FFTEngine> videoFftEngine;
    std::unique_ptr<rf::FilterBank> videoFilterBank;
    
    // IRE conversion parameters
    float ire0Hz;
    float hzPerIre;
    float vsyncIre;
    float outputZero;
    float outScale;

    Impl(const Config& cfg) : config(cfg) {
        // Initialize RF Processor
        rf::RFProcessor::Config rfConfig;
        rfConfig.sampleRateMHz = config.inputFreqMHz;
        rfConfig.blockSize = config.blockSize;
        rfConfig.useGPU = config.useGPU;
        
        // Default bandpass (will be updated by setBandpassFrequencies or defaults)
        // Using PAL defaults for now as in main.cpp
        rfConfig.rfBandpassLowMHz = 1.3;
        rfConfig.rfBandpassHighMHz = 5.78;
        
        rfProcessor = std::make_unique<rf::RFProcessor>(rfConfig);
        
        // Create format config to get system params
        auto formatImpl = formats::createFormat(config.format);
        auto fullConfig = formatImpl->createConfig(config.system, config.inputFreqMHz);

        // Initialize FM Demodulator
        ProcessingConfig demodConfig;
        demodConfig.inputFreqMHz = config.inputFreqMHz;
        demodConfig.blockSize = config.blockSize;
        demodConfig.system = fullConfig.system;
        demodConfig.useGPU = config.useGPU;
        
        fmDemod = std::make_unique<demod::FMDemodulator>(demodConfig);
        
        // Initialize Video Filtering
        videoFftEngine = std::make_unique<rf::FFTEngine>(config.blockSize, false); // CPU for now
        videoFilterBank = std::make_unique<rf::FilterBank>(config.inputFreqMHz, config.blockSize);
        
        // Setup default video filters (PAL/NTSC)
        bool isPAL = (config.system == TVSystem::PAL);
        double videoLpfCutoffMHz = isPAL ? 3.4 : 6.6;
        int videoLpfOrder = 6;
        videoFilterBank->createButterworthLPF("video_lpf", videoLpfCutoffMHz, videoLpfOrder);
        
        double deemphGain = 13.9794;
        double deemphMid = 273755.82;
        double deemphQ = 0.462088186;
        videoFilterBank->createDeemphasisFilter("video_deemph", deemphGain, deemphMid, deemphQ);
        
        // Initialize IRE parameters
        ire0Hz = 4100000.0f;
        hzPerIre = 7000.0f;
        vsyncIre = -42.857f;
        outputZero = 256.0f;
        outScale = 53760.0f / 142.857f;
    }
    
    void process(const std::vector<uint8_t>& inputBlock, 
                 std::vector<uint16_t>& videoOutput,
                 std::vector<uint16_t>& chromaOutput) {
        
        // 1. RF Processing
        auto rfResult = rfProcessor->processBlock(inputBlock);
        
        // 2. FM Demodulation
        auto fmResult = fmDemod->demodulate(rfResult.analyticSignal);
        
        // 3. Spike Replacement
        fmDemod->replaceSpikes(fmResult.video, rfResult.analyticSignal);
        
        // 4. Video Filtering
        auto videoFft = videoFftEngine->forwardFFT(fmResult.video);
        const auto& deemph = videoFilterBank->getFilter("video_deemph");
        const auto& lpf = videoFilterBank->getFilter("video_lpf");
        
        videoFftEngine->applyFilter(videoFft, deemph);
        videoFftEngine->applyFilter(videoFft, lpf);
        
        fmResult.video = videoFftEngine->inverseFFT(videoFft);
        
        // 5. Convert to Digital (IRE -> uint16)
        for (float freqHz : fmResult.video) {
            float ire = (freqHz - ire0Hz) / hzPerIre;
            float digital = (ire - vsyncIre) * outScale + outputZero;
            digital = std::max(0.0f, std::min(65535.0f, digital));
            videoOutput.push_back(static_cast<uint16_t>(digital));
        }
        
        // Chroma (placeholder conversion)
        for (float freqHz : fmResult.chroma) {
            float ire = (freqHz - ire0Hz) / hzPerIre;
            float digital = (ire - vsyncIre) * outScale + outputZero;
            digital = std::max(0.0f, std::min(65535.0f, digital));
            chromaOutput.push_back(static_cast<uint16_t>(digital));
        }
    }
};

Pipeline::Pipeline(const Config& config)
    : impl_(std::make_unique<Impl>(config))
{
}

Pipeline::~Pipeline() = default;

void Pipeline::setBandpassFrequencies(double lowMHz, double highMHz) {
    impl_->rfProcessor->setBandpassFrequencies(lowMHz, highMHz);
}

void Pipeline::setSpikeThreshold(float mhz) {
    impl_->fmDemod->setSpikeThreshold(mhz);
}

void Pipeline::setSpikeReplacement(bool enable) {
    impl_->fmDemod->setSpikeReplacement(enable);
}

void Pipeline::setVideoLPF(double cutoffMHz, int order) {
    impl_->videoFilterBank->createButterworthLPF("video_lpf", cutoffMHz, order);
}

void Pipeline::setDeemphasis(double gain, double midFreqHz, double q) {
    impl_->videoFilterBank->createDeemphasisFilter("video_deemph", gain, midFreqHz, q);
}

bool Pipeline::processBlock(const std::vector<uint8_t>& inputBlock, 
                            std::vector<uint16_t>& videoOutput,
                            std::vector<uint16_t>& chromaOutput) {
    try {
        impl_->process(inputBlock, videoOutput, chromaOutput);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Pipeline error: " << e.what() << std::endl;
        return false;
    }
}

rf::RFProcessor& Pipeline::getRFProcessor() {
    return *impl_->rfProcessor;
}

demod::FMDemodulator& Pipeline::getFMDemodulator() {
    return *impl_->fmDemod;
}

} // namespace vhsdecode
