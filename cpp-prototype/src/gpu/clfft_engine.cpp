#ifdef HAVE_CLFFT

#include "vhsdecode/clfft_engine.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace vhsdecode {
namespace gpu {

// Static members
bool CLFFTEngine::clFFTInitialized_ = false;
int CLFFTEngine::instanceCount_ = 0;

// OpenCL kernel for filter application
const char* filterKernelSource = R"(
__kernel void apply_filter_freq_domain(
    __global double2* fft_data,           // Complex FFT data (in/out)
    __global const double* filter_response,  // Real filter response
    const int N
) {
    int idx = get_global_id(0);
    
    if (idx < N) {
        double2 data = fft_data[idx];
        double gain = filter_response[idx];
        
        fft_data[idx].x = data.x * gain;
        fft_data[idx].y = data.y * gain;
    }
}
)";

void CLFFTEngine::initializeClFFT() {
    if (!clFFTInitialized_) {
        clfftSetupData setupData;
        clfftInitSetupData(&setupData);
        
        clfftStatus status = clfftSetup(&setupData);
        if (status != CLFFT_SUCCESS) {
            throw std::runtime_error("Failed to initialize clFFT library");
        }
        
        clFFTInitialized_ = true;
        std::cout << "clFFT library initialized" << std::endl;
    }
    instanceCount_++;
}

void CLFFTEngine::cleanupClFFT() {
    instanceCount_--;
    if (instanceCount_ == 0 && clFFTInitialized_) {
        clfftTeardown();
        clFFTInitialized_ = false;
        std::cout << "clFFT library cleaned up" << std::endl;
    }
}

CLFFTEngine::CLFFTEngine(OpenCLContext& ctx, size_t blockSize, bool useGPU)
    : ctx_(ctx)
    , blockSize_(blockSize)
    , batchSize_(1)
    , useGPU_(useGPU)
    , forwardPlan_(0)
    , inversePlan_(0)
{
    try {
        // Initialize clFFT library
        initializeClFFT();
        
        // Create FFT plans
        createForwardPlan();
        createInversePlan();
        
        // Create temporary buffers
        size_t realBufferSize = blockSize_ * sizeof(double);
        size_t complexBufferSize = (blockSize_ / 2 + 1) * sizeof(cl_double2);
        
        tempRealBuffer_ = ctx_.createBuffer(realBufferSize, CL_MEM_READ_WRITE);
        tempComplexBuffer_ = ctx_.createBuffer(complexBufferSize, CL_MEM_READ_WRITE);
        
        // Initialize filter kernel
        initializeFilterKernel();
        
        std::cout << "CLFFTEngine created (block size: " << blockSize_ << ")" << std::endl;
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error in CLFFTEngine constructor: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss << "Error in CLFFTEngine constructor: " << e.what();
        throw std::runtime_error(ss.str());
    }
}

CLFFTEngine::~CLFFTEngine() {
    try {
        if (forwardPlan_ != 0) {
            clfftDestroyPlan(&forwardPlan_);
        }
        if (inversePlan_ != 0) {
            clfftDestroyPlan(&inversePlan_);
        }
        cleanupClFFT();
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

CLFFTEngine::CLFFTEngine(CLFFTEngine&& other) noexcept
    : ctx_(other.ctx_)
    , blockSize_(other.blockSize_)
    , batchSize_(other.batchSize_)
    , useGPU_(other.useGPU_)
    , forwardPlan_(other.forwardPlan_)
    , inversePlan_(other.inversePlan_)
    , tempRealBuffer_(std::move(other.tempRealBuffer_))
    , tempComplexBuffer_(std::move(other.tempComplexBuffer_))
    , filterKernel_(std::move(other.filterKernel_))
    , filterProgram_(std::move(other.filterProgram_))
{
    other.forwardPlan_ = 0;
    other.inversePlan_ = 0;
}

CLFFTEngine& CLFFTEngine::operator=(CLFFTEngine&& other) noexcept {
    if (this != &other) {
        // Cleanup existing plans
        if (forwardPlan_ != 0) {
            clfftDestroyPlan(&forwardPlan_);
        }
        if (inversePlan_ != 0) {
            clfftDestroyPlan(&inversePlan_);
        }
        
        // Move data
        ctx_ = other.ctx_;
        blockSize_ = other.blockSize_;
        batchSize_ = other.batchSize_;
        useGPU_ = other.useGPU_;
        forwardPlan_ = other.forwardPlan_;
        inversePlan_ = other.inversePlan_;
        tempRealBuffer_ = std::move(other.tempRealBuffer_);
        tempComplexBuffer_ = std::move(other.tempComplexBuffer_);
        filterKernel_ = std::move(other.filterKernel_);
        filterProgram_ = std::move(other.filterProgram_);
        
        other.forwardPlan_ = 0;
        other.inversePlan_ = 0;
    }
    return *this;
}

void CLFFTEngine::createForwardPlan() {
    size_t lengths[1] = { blockSize_ };
    
    clfftStatus status = clfftCreateDefaultPlan(
        &forwardPlan_,
        ctx_.getContext()(),
        CLFFT_1D,
        lengths
    );
    
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to create forward FFT plan");
    }
    
    // Set plan parameters
    status = clfftSetPlanPrecision(forwardPlan_, CLFFT_DOUBLE);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to set FFT precision to double");
    }
    
    status = clfftSetLayout(forwardPlan_, CLFFT_REAL, CLFFT_HERMITIAN_INTERLEAVED);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to set forward FFT layout");
    }
    
    status = clfftSetResultLocation(forwardPlan_, CLFFT_OUTOFPLACE);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to set forward FFT result location");
    }
    
    // Bake the plan
    cl_command_queue queue = ctx_.getQueue()();
    status = clfftBakePlan(forwardPlan_, 1, &queue, nullptr, nullptr);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to bake forward FFT plan");
    }
}

void CLFFTEngine::createInversePlan() {
    size_t lengths[1] = { blockSize_ };
    
    clfftStatus status = clfftCreateDefaultPlan(
        &inversePlan_,
        ctx_.getContext()(),
        CLFFT_1D,
        lengths
    );
    
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to create inverse FFT plan");
    }
    
    // Set plan parameters
    status = clfftSetPlanPrecision(inversePlan_, CLFFT_DOUBLE);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to set iFFT precision to double");
    }
    
    status = clfftSetLayout(inversePlan_, CLFFT_HERMITIAN_INTERLEAVED, CLFFT_REAL);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to set inverse FFT layout");
    }
    
    status = clfftSetResultLocation(inversePlan_, CLFFT_OUTOFPLACE);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to set inverse FFT result location");
    }
    
    // Bake the plan
    cl_command_queue queue = ctx_.getQueue()();
    status = clfftBakePlan(inversePlan_, 1, &queue, nullptr, nullptr);
    if (status != CLFFT_SUCCESS) {
        throw std::runtime_error("Failed to bake inverse FFT plan");
    }
}

void CLFFTEngine::initializeFilterKernel() {
    try {
        // Build OpenCL program
        cl::Program::Sources sources;
        sources.push_back({filterKernelSource, strlen(filterKernelSource)});
        
        filterProgram_ = cl::Program(ctx_.getContext(), sources);
        filterProgram_.build({ctx_.getDevice()});
        
        // Create kernel
        filterKernel_ = cl::Kernel(filterProgram_, "apply_filter_freq_domain");
        
    } catch (const cl::Error& e) {
        if (e.err() == CL_BUILD_PROGRAM_FAILURE) {
            std::string buildLog = filterProgram_.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ctx_.getDevice());
            std::stringstream ss;
            ss << "OpenCL kernel build failed:\n" << buildLog;
            throw std::runtime_error(ss.str());
        }
        throw;
    }
}

std::vector<std::complex<double>> CLFFTEngine::forwardFFT(const std::vector<double>& input) {
    if (input.size() != blockSize_) {
        throw std::invalid_argument("Input size must match block size");
    }
    
    try {
        // Transfer input to GPU
        ctx_.writeBuffer(tempRealBuffer_, input.data(), blockSize_ * sizeof(double));
        
        // Execute forward FFT
        cl_command_queue queue = ctx_.getQueue()();
        cl_mem buffers[2] = { tempRealBuffer_(), tempComplexBuffer_() };
        
        clfftStatus status = clfftEnqueueTransform(
            forwardPlan_,
            CLFFT_FORWARD,
            1,
            &queue,
            0,
            nullptr,
            nullptr,
            buffers,
            &buffers[1],
            nullptr
        );
        
        if (status != CLFFT_SUCCESS) {
            throw std::runtime_error("Failed to execute forward FFT");
        }
        
        // Wait for completion
        ctx_.getQueue().finish();
        
        // Transfer result back to host
        size_t outputSize = blockSize_ / 2 + 1;
        std::vector<cl_double2> tempOutput(outputSize);
        ctx_.readBuffer(tempComplexBuffer_, tempOutput.data(), outputSize * sizeof(cl_double2));
        
        // Convert cl_double2 to std::complex<double>
        std::vector<std::complex<double>> output(outputSize);
        for (size_t i = 0; i < outputSize; ++i) {
            output[i] = std::complex<double>(tempOutput[i].s[0], tempOutput[i].s[1]);
        }
        
        return output;
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error in forwardFFT: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

std::vector<double> CLFFTEngine::inverseFFT(const std::vector<std::complex<double>>& input) {
    size_t expectedSize = blockSize_ / 2 + 1;
    if (input.size() != expectedSize) {
        throw std::invalid_argument("Input size must be blockSize/2+1");
    }
    
    try {
        // Convert std::complex<double> to cl_double2
        std::vector<cl_double2> tempInput(expectedSize);
        for (size_t i = 0; i < expectedSize; ++i) {
            tempInput[i].s[0] = input[i].real();
            tempInput[i].s[1] = input[i].imag();
        }
        
        // Transfer input to GPU
        ctx_.writeBuffer(tempComplexBuffer_, tempInput.data(), expectedSize * sizeof(cl_double2));
        
        // Execute inverse FFT
        cl_command_queue queue = ctx_.getQueue()();
        cl_mem buffers[2] = { tempComplexBuffer_(), tempRealBuffer_() };
        
        clfftStatus status = clfftEnqueueTransform(
            inversePlan_,
            CLFFT_BACKWARD,
            1,
            &queue,
            0,
            nullptr,
            nullptr,
            buffers,
            &buffers[1],
            nullptr
        );
        
        if (status != CLFFT_SUCCESS) {
            throw std::runtime_error("Failed to execute inverse FFT");
        }
        
        // Wait for completion
        ctx_.getQueue().finish();
        
        // Transfer result back to host
        std::vector<double> output(blockSize_);
        ctx_.readBuffer(tempRealBuffer_, output.data(), blockSize_ * sizeof(double));
        
        // clFFT doesn't normalize, so we need to divide by N
        double scale = 1.0 / blockSize_;
        for (double& val : output) {
            val *= scale;
        }
        
        return output;
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error in inverseFFT: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

void CLFFTEngine::forwardFFTGPU(
    cl::Buffer& inputBuffer,
    cl::Buffer& outputBuffer,
    bool keepOnDevice
) {
    try {
        cl_command_queue queue = ctx_.getQueue()();
        cl_mem buffers[2] = { inputBuffer(), outputBuffer() };
        
        clfftStatus status = clfftEnqueueTransform(
            forwardPlan_,
            CLFFT_FORWARD,
            1,
            &queue,
            0,
            nullptr,
            nullptr,
            buffers,
            &buffers[1],
            nullptr
        );
        
        if (status != CLFFT_SUCCESS) {
            throw std::runtime_error("Failed to execute GPU forward FFT");
        }
        
        if (!keepOnDevice) {
            ctx_.getQueue().finish();
        }
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error in forwardFFTGPU: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

void CLFFTEngine::inverseFFTGPU(
    cl::Buffer& inputBuffer,
    cl::Buffer& outputBuffer,
    bool keepOnDevice
) {
    try {
        cl_command_queue queue = ctx_.getQueue()();
        cl_mem buffers[2] = { inputBuffer(), outputBuffer() };
        
        clfftStatus status = clfftEnqueueTransform(
            inversePlan_,
            CLFFT_BACKWARD,
            1,
            &queue,
            0,
            nullptr,
            nullptr,
            buffers,
            &buffers[1],
            nullptr
        );
        
        if (status != CLFFT_SUCCESS) {
            throw std::runtime_error("Failed to execute GPU inverse FFT");
        }
        
        if (!keepOnDevice) {
            ctx_.getQueue().finish();
        }
        
        // TODO: Add normalization kernel if needed
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error in inverseFFTGPU: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

void CLFFTEngine::applyFilterGPU(
    cl::Buffer& fftData,
    cl::Buffer& filter,
    size_t filterSize
) {
    try {
        // Set kernel arguments
        filterKernel_.setArg(0, fftData);
        filterKernel_.setArg(1, filter);
        filterKernel_.setArg(2, static_cast<int>(filterSize));
        
        // Determine work group size
        size_t maxWorkGroupSize;
        filterKernel_.getWorkGroupInfo(
            ctx_.getDevice(),
            CL_KERNEL_WORK_GROUP_SIZE,
            &maxWorkGroupSize
        );
        
        size_t globalSize = ((filterSize + 255) / 256) * 256;  // Round up to multiple of 256
        size_t localSize = std::min(static_cast<size_t>(256), maxWorkGroupSize);
        
        // Execute kernel
        ctx_.getQueue().enqueueNDRangeKernel(
            filterKernel_,
            cl::NullRange,
            cl::NDRange(globalSize),
            cl::NDRange(localSize)
        );
        
        ctx_.getQueue().finish();
        
    } catch (const cl::Error& e) {
        std::stringstream ss;
        ss << "OpenCL error in applyFilterGPU: " << e.what() 
           << " (code: " << e.err() << ")";
        throw std::runtime_error(ss.str());
    }
}

void CLFFTEngine::setBatchSize(size_t batchSize) {
    if (batchSize < 1) {
        throw std::invalid_argument("Batch size must be at least 1");
    }
    batchSize_ = batchSize;
    
    // TODO: Recreate plans for batch processing if batchSize > 1
    // For now, batch size is stored but not used
}

} // namespace gpu
} // namespace vhsdecode

#endif // HAVE_CLFFT
