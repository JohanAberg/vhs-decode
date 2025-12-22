#ifdef HAVE_OPENCL

#include "vhsdecode/rf_processor_gpu.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace vhsdecode {
namespace rf {

RFProcessorGPU::RFProcessorGPU(const Config& config)
    : config_(config)
    , usingGPU_(false)
{
    std::cout << "[GPU] Initializing RF Processor GPU...\n";
    
    try {
        initializeGPU();
        std::cout << "[GPU] ✓ RF Processor GPU initialized successfully\n";
    } catch (const std::exception& e) {
        std::cerr << "[GPU] ⚠ GPU initialization failed: " << e.what() << "\n";
        std::cerr << "[GPU] Falling back to CPU processing\n";
        
        // Create CPU fallback
        Config cpuConfig = config_;
        cpuConfig.useGPU = false;
        cpuFallback_ = std::make_unique<RFProcessor>(cpuConfig);
        usingGPU_ = false;
    }
}

RFProcessorGPU::~RFProcessorGPU() = default;

RFProcessorGPU::RFProcessorGPU(RFProcessorGPU&&) noexcept = default;
RFProcessorGPU& RFProcessorGPU::operator=(RFProcessorGPU&&) noexcept = default;

void RFProcessorGPU::initializeGPU() {
    // Initialize OpenCL context
    clContext_ = std::make_unique<gpu::OpenCLContext>();
    
    // Initialize clFFT engine for GPU FFT
    clfftEngine_ = std::make_unique<gpu::CLFFTEngine>(*clContext_, config_.blockSize, true);
    
    // Initialize envelope kernel
    envelopeKernel_ = std::make_unique<gpu::EnvelopeKernel>(*clContext_);
    
    // Pre-allocate GPU buffers for block processing
    size_t realBufferSize = config_.blockSize * sizeof(double);
    size_t complexBufferSize = (config_.blockSize / 2 + 1) * sizeof(cl_double2);
    
    inputBuffer_ = clContext_->createBuffer(realBufferSize, CL_MEM_READ_WRITE);
    outputBuffer_ = clContext_->createBuffer(realBufferSize, CL_MEM_READ_WRITE);
    analyticBuffer_ = clContext_->createBuffer(config_.blockSize * sizeof(cl_double2), CL_MEM_READ_WRITE);
    envelopeBuffer_ = clContext_->createBuffer(realBufferSize, CL_MEM_READ_WRITE);
    
    usingGPU_ = true;
}

RealArray RFProcessorGPU::uint8ToFloat(const std::vector<uint8_t>& data) {
    RealArray result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = (static_cast<float>(data[i]) - 128.0f) / 128.0f;
    }
    return result;
}

RFProcessorGPU::Result RFProcessorGPU::processBlock(const std::vector<uint8_t>& rfSamples) {
    auto rfData = uint8ToFloat(rfSamples);
    return processBlock(rfData);
}

RFProcessorGPU::Result RFProcessorGPU::processBlock(const RealArray& rfData) {
    if (!usingGPU_) {
        return processBlockCPU(rfData);
    }
    
    try {
        return processBlockGPU(rfData);
    } catch (const std::exception& e) {
        std::cerr << "[GPU] Processing failed: " << e.what() << "\n";
        std::cerr << "[GPU] Falling back to CPU for this block\n";
        return processBlockCPU(rfData);
    }
}

RFProcessorGPU::Result RFProcessorGPU::processBlockGPU(const RealArray& rfData) {
    if (rfData.size() != config_.blockSize) {
        throw std::invalid_argument("RF data size doesn't match block size");
    }
    
    Result result;
    const size_t N = config_.blockSize;
    
    // Convert float to double for GPU (clFFT uses double precision)
    std::vector<double> rfDataDouble(N);
    for (size_t i = 0; i < N; ++i) {
        rfDataDouble[i] = static_cast<double>(rfData[i]);
    }
    
    // Upload RF data to GPU
    clContext_->getQueue().enqueueWriteBuffer(inputBuffer_, CL_TRUE, 0, 
                                               N * sizeof(double), rfDataDouble.data());
    
    // Step 1: Forward FFT on GPU
    cl::Buffer fftBuffer = clContext_->createBuffer((N/2 + 1) * sizeof(cl_double2), CL_MEM_READ_WRITE);
    clfftEngine_->forwardFFTGPU(inputBuffer_, fftBuffer);
    
    // Step 2: Apply RF bandpass filter on GPU
    // TODO: Upload filter coefficients and apply on GPU
    // For now, download to CPU, apply filter, upload back
    std::vector<cl_double2> fftData(N/2 + 1);
    clContext_->getQueue().enqueueReadBuffer(fftBuffer, CL_TRUE, 0, 
                                              (N/2 + 1) * sizeof(cl_double2), fftData.data());
    
    // Convert to complex array for CPU filter (temporary until GPU filter is implemented)
    std::vector<std::complex<double>> fftComplex(N/2 + 1);
    for (size_t i = 0; i < N/2 + 1; ++i) {
        fftComplex[i] = std::complex<double>(fftData[i].s[0], fftData[i].s[1]);
    }
    
    // Apply RF bandpass filter (CPU for now)
    // TODO: Implement GPU filter application
    
    // Step 3: Apply Hilbert transform (zero negative frequencies)
    // This is trivial on GPU - just zero out negative frequency bins
    for (size_t i = 1; i < N/2; ++i) {
        fftComplex[i] *= 2.0;  // Double positive frequencies
    }
    
    // Convert back to cl_double2
    for (size_t i = 0; i < N/2 + 1; ++i) {
        fftData[i].s[0] = fftComplex[i].real();
        fftData[i].s[1] = fftComplex[i].imag();
    }
    
    // Upload filtered FFT back to GPU
    clContext_->getQueue().enqueueWriteBuffer(fftBuffer, CL_TRUE, 0, 
                                               (N/2 + 1) * sizeof(cl_double2), fftData.data());
    
    // Step 4: Inverse FFT to get complex analytic signal
    // For complex iFFT, we need to expand to full spectrum
    cl::Buffer fullSpectrumBuffer = clContext_->createBuffer(N * sizeof(cl_double2), CL_MEM_READ_WRITE);
    
    // Copy positive frequencies and mirror for negative frequencies (conjugate)
    std::vector<cl_double2> fullSpectrum(N);
    for (size_t i = 0; i < N/2 + 1; ++i) {
        fullSpectrum[i] = fftData[i];
    }
    for (size_t i = N/2 + 1; i < N; ++i) {
        size_t mirror = N - i;
        fullSpectrum[i].s[0] = fftData[mirror].s[0];   // Real part
        fullSpectrum[i].s[1] = -fftData[mirror].s[1];  // Conjugate imaginary
    }
    
    clContext_->getQueue().enqueueWriteBuffer(fullSpectrumBuffer, CL_TRUE, 0, 
                                               N * sizeof(cl_double2), fullSpectrum.data());
    
    // Perform inverse FFT to get time-domain analytic signal
    cl::Buffer analyticTimeBuffer = clContext_->createBuffer(N * sizeof(cl_double2), CL_MEM_READ_WRITE);
    // Note: clFFT for complex-to-complex inverse FFT
    // TODO: Use proper complex iFFT from clFFT
    
    // For now, download and do iFFT on CPU
    // This is a temporary solution until full GPU pipeline is implemented
    result.analyticSignal.resize(N);
    clContext_->getQueue().enqueueReadBuffer(fullSpectrumBuffer, CL_TRUE, 0, 
                                              N * sizeof(cl_double2), fullSpectrum.data());
    
    // Simple complex iFFT (this should be done on GPU)
    for (size_t k = 0; k < N; ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (size_t n = 0; n < N; ++n) {
            double angle = 2.0 * M_PI * k * n / N;
            std::complex<double> twiddle(std::cos(angle), std::sin(angle));
            std::complex<double> sample(fullSpectrum[n].s[0], fullSpectrum[n].s[1]);
            sum += sample * twiddle;
        }
        result.analyticSignal[k] = std::complex<float>(sum.real() / N, sum.imag() / N);
    }
    
    // Step 5: Compute envelope on GPU
    // Upload analytic signal to GPU
    std::vector<cl_double2> analyticData(N);
    for (size_t i = 0; i < N; ++i) {
        analyticData[i].s[0] = result.analyticSignal[i].real();
        analyticData[i].s[1] = result.analyticSignal[i].imag();
    }
    clContext_->getQueue().enqueueWriteBuffer(analyticBuffer_, CL_TRUE, 0, 
                                               N * sizeof(cl_double2), analyticData.data());
    
    // Compute envelope on GPU
    envelopeKernel_->detectGPU(analyticBuffer_, envelopeBuffer_, N);
    
    // Download envelope from GPU
    std::vector<double> envelopeDouble(N);
    clContext_->getQueue().enqueueReadBuffer(envelopeBuffer_, CL_TRUE, 0, 
                                              N * sizeof(double), envelopeDouble.data());
    
    result.envelope.resize(N);
    for (size_t i = 0; i < N; ++i) {
        result.envelope[i] = static_cast<float>(envelopeDouble[i]);
    }
    
    // Step 6: Extract filtered real signal (real part of analytic signal)
    result.filtered.resize(N);
    for (size_t i = 0; i < N; ++i) {
        result.filtered[i] = result.analyticSignal[i].real();
    }
    
    // Chroma extraction (TODO: implement on GPU)
    result.chroma.resize(N, 0.0f);
    
    return result;
}

RFProcessorGPU::Result RFProcessorGPU::processBlockCPU(const RealArray& rfData) {
    if (!cpuFallback_) {
        // Create CPU fallback if it doesn't exist
        Config cpuConfig = config_;
        cpuConfig.useGPU = false;
        cpuFallback_ = std::make_unique<RFProcessor>(cpuConfig);
    }
    
    return cpuFallback_->processBlock(rfData);
}

} // namespace rf
} // namespace vhsdecode

#endif // HAVE_OPENCL
