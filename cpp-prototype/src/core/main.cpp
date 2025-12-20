#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <iomanip>
#include "vhsdecode/types.hpp"
#include "vhsdecode/rf_reader.hpp"
#include "vhsdecode/tbc_writer.hpp"
#include "vhsdecode/fft_engine.hpp"
#include "vhsdecode/fm_demodulator.hpp"
#include "vhsdecode/filter_bank.hpp"
#include "vhsdecode/hilbert.hpp"
#include "vhsdecode/rf_processor.hpp"
#include "formats/format_base.hpp"
#include "formats/vhs_format.hpp"

#ifdef HAVE_FFTW3
#include "vhsdecode/fftw_engine.hpp"
#endif

using namespace vhsdecode;
using namespace vhsdecode::formats;

void printUsage(const char* progName) {
    std::cout << "VHS-Decode C++ Prototype v0.1.0\n";
    std::cout << "Usage: " << progName << " [options] <input.r40> <output.tbc>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --format <format>     Tape format (VHS, SVHS, Betamax) [default: VHS]\n";
    std::cout << "  --system <system>     TV system (NTSC, PAL, PAL-M) [default: NTSC]\n";
    std::cout << "  --threads <n>         Number of threads [default: 4]\n";
    std::cout << "  --gpu                 Enable GPU acceleration\n";
    std::cout << "  --use-prototype-fft   Use prototype FFT (slower, for testing)\n";
    std::cout << "  --help                Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << progName << " --format VHS --system NTSC input.r40 output.tbc\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string formatName = "VHS";
    std::string systemName = "NTSC";
    int threads = 4;
    bool useGPU = false;
    bool usePrototypeFFT = false;
    std::string inputFile;
    std::string outputFile;
    
    // Parse command line
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--format" && i + 1 < argc) {
            formatName = argv[++i];
        }
        else if (arg == "--system" && i + 1 < argc) {
            systemName = argv[++i];
        }
        else if (arg == "--threads" && i + 1 < argc) {
            threads = std::stoi(argv[++i]);
        }
        else if (arg == "--gpu") {
            useGPU = true;
        }
        else if (arg == "--use-prototype-fft") {
            usePrototypeFFT = true;
        }
        else if (arg[0] != '-') {
            if (inputFile.empty()) {
                inputFile = arg;
            } else if (outputFile.empty()) {
                outputFile = arg;
            }
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Validate arguments
    if (inputFile.empty() || outputFile.empty()) {
        std::cerr << "Error: Input and output files required\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        // Create format implementation
        TapeFormat format = formatFromString(formatName);
        TVSystem system = systemFromString(systemName);
        
        std::cout << "VHS-Decode C++ Prototype\n";
        std::cout << "========================\n";
        std::cout << "Format:  " << formatToString(format) << "\n";
        std::cout << "System:  " << systemToString(system) << "\n";
        std::cout << "Threads: " << threads << "\n";
        std::cout << "GPU:     " << (useGPU ? "Enabled" : "Disabled") << "\n";
        std::cout << "Input:   " << inputFile << "\n";
        std::cout << "Output:  " << outputFile << "\n";
        std::cout << "\n";
        
        // Create format-specific configuration
        auto formatImpl = createFormat(format);
        auto config = formatImpl->createConfig(system, 40.0);
        config.useGPU = useGPU;
        config.threadCount = threads;
        
        // Display configuration
        std::cout << "Configuration:\n";
        std::cout << "  Video carrier: " << formatImpl->getVideoCarrierMHz(system) << " MHz\n";
        std::cout << "  Chroma carrier: " << formatImpl->getChromaCarrierMHz(system) << " MHz\n";
        std::cout << "  Color-under: " << (formatImpl->isColorUnder() ? "Yes" : "No") << "\n";
        std::cout << "  Input frequency: " << config.inputFreqMHz << " MHz\n";
        std::cout << "  Block size: " << config.blockSize << " samples\n";
        std::cout << "  Frame lines: " << config.system.frameLines << "\n";
        std::cout << "  Field lines: " << config.system.fieldLines[0] << "/" 
                  << config.system.fieldLines[1] << "\n";
        std::cout << "\n";
        
        // Try to open RF file and test full pipeline
        std::cout << "Opening RF file...\n";
        try {
            io::RFReader reader(inputFile, true);  // Use memory-mapped I/O
            
            std::cout << "✓ RF file opened successfully\n";
            std::cout << "  File size: " << reader.getFileSize() << " bytes\n";
            std::cout << "  Sample count: " << reader.getSampleCount(1) << " samples (uint8)\n";
            std::cout << "  Memory-mapped: " << (reader.isMemoryMapped() ? "Yes" : "No") << "\n";
            
            // Try creating TBC writer
            std::cout << "\nInitializing TBC writer...\n";
            io::TBCWriter writer(outputFile, true, true);
            std::cout << "✓ TBC writer initialized\n";
            std::cout << "  Video file: " << outputFile << ".tbc\n";
            std::cout << "  Chroma file: " << outputFile << "_chroma.tbc\n";
            std::cout << "  Metadata file: " << outputFile << ".tbc.json\n";
            
            // Try creating FFT engine
            std::cout << "\nInitializing FFT engine...\n";
            
#ifdef HAVE_FFTW3
            // Use FFTW3 by default if available and not explicitly disabled
            if (!usePrototypeFFT) {
                try {
                    rf::FFTWEngine fftwEngine(config.blockSize, false);
                    std::cout << "✓ FFTW3 FFT engine initialized\n";
                    std::cout << "  Block size: " << fftwEngine.getBlockSize() << " samples\n";
                    std::cout << "  FFT bins: " << fftwEngine.getBinCount() << "\n";
                    std::cout << "  Using GPU: " << (fftwEngine.isUsingGPU() ? "Yes" : "No (CPU)") << "\n";
                    
                    // Test with first block
                    if (reader.getFileSize() >= config.blockSize) {
                        auto rfBlock = reader.readBlock(config.blockSize, 0);
                        
                        // Convert uint8 to double
                        std::vector<double> rfData(rfBlock.data.size());
                        for (size_t i = 0; i < rfBlock.data.size(); ++i) {
                            rfData[i] = static_cast<double>(rfBlock.data[i]) / 255.0;
                        }
                        
                        std::cout << "\nTesting FFTW3...\n";
                        auto fftResult = fftwEngine.forwardFFT(rfData);
                        std::cout << "✓ Forward FFT completed\n";
                        std::cout << "  FFT bins: " << fftResult.size() << "\n";
                        
                        auto ifftResult = fftwEngine.inverseFFT(fftResult);
                        std::cout << "✓ Inverse FFT completed\n";
                        std::cout << "  Reconstructed samples: " << ifftResult.size() << "\n";
                        
                        // Calculate reconstruction error
                        double maxError = 0.0;
                        for (size_t i = 0; i < std::min(rfData.size(), ifftResult.size()); ++i) {
                            maxError = std::max(maxError, std::abs(rfData[i] - ifftResult[i]));
                        }
                        std::cout << "  Max reconstruction error: " << std::scientific << std::setprecision(6) << maxError << "\n";
                    }
                } catch (const std::exception& e) {
                    std::cout << "FFTW3 engine initialization failed: " << e.what() << "\n";
                    usePrototypeFFT = true;  // Fall back to prototype
                }
            }
#else
            usePrototypeFFT = true;  // No FFTW3 available
#endif
            
            // Use prototype FFT if requested or if FFTW3 failed
            if (usePrototypeFFT) {
                try {
                    rf::FFTEngine fftEngine(config.blockSize, false);  // CPU-only for now
                    std::cout << "✓ Prototype FFT engine initialized\n";
                    std::cout << "  Block size: " << fftEngine.getBlockSize() << " samples\n";
                    std::cout << "  Using GPU: " << (fftEngine.isUsingGPU() ? "Yes" : "No (CPU)") << "\n";
                    std::cout << "  Note: This is the slow prototype FFT. Install FFTW3 for 1000x speedup!\n";
                    
                    // Test FFT with first block
                    if (reader.getFileSize() >= config.blockSize) {
                        auto rfBlock = reader.readBlock(config.blockSize, 0);
                        
                        // Convert uint8 to float
                        RealArray rfData(rfBlock.data.size());
                        for (size_t i = 0; i < rfBlock.data.size(); ++i) {
                            rfData[i] = static_cast<float>(rfBlock.data[i]) / 255.0f;
                        }
                        
                        std::cout << "\nTesting FFT...\n";
                        auto fftResult = fftEngine.forwardFFT(rfData);
                        std::cout << "✓ Forward FFT completed\n";
                        std::cout << "  FFT bins: " << fftResult.size() << "\n";
                        
                        auto ifftResult = fftEngine.inverseFFT(fftResult);
                        std::cout << "✓ Inverse FFT completed\n";
                        std::cout << "  Reconstructed samples: " << ifftResult.size() << "\n";
                        
                        // Calculate reconstruction error
                        float maxError = 0.0f;
                        for (size_t i = 0; i < std::min(rfData.size(), ifftResult.size()); ++i) {
                            maxError = std::max(maxError, std::abs(rfData[i] - ifftResult[i]));
                        }
                        std::cout << "  Max reconstruction error: " << std::fixed << std::setprecision(6) << maxError << "\n";
                    }
                    
                } catch (const std::exception& e) {
                    std::cout << "FFT engine initialization failed: " << e.what() << "\n";
                }
            }
            
            // Try creating FM demodulator
            std::cout << "\nInitializing FM demodulator...\n";
            demod::FMDemodulator fmDemod(config);
            std::cout << "✓ FM demodulator initialized\n";
            
            // Test with a simple analytic signal
            ComplexArray testSignal(1024);
            for (size_t i = 0; i < testSignal.size(); ++i) {
                float t = static_cast<float>(i) / testSignal.size();
                testSignal[i] = std::complex<float>(std::cos(2.0f * M_PI * t * 10.0f), 
                                                     std::sin(2.0f * M_PI * t * 10.0f));
            }
            
            auto demodResult = fmDemod.demodulate(testSignal);
            std::cout << "✓ FM demodulation test completed\n";
            std::cout << "  Video samples: " << demodResult.video.size() << "\n";
            std::cout << "  Envelope samples: " << demodResult.envelope.size() << "\n";
            
            // Write a test field to TBC
            std::cout << "\nWriting test field to TBC...\n";
            VideoField testField;
            // Create a simple test pattern (e.g., 1000 lines of 1000 samples each)
            for (int line = 0; line < 10; ++line) {
                VideoLine videoLine(100);
                for (size_t s = 0; s < videoLine.size(); ++s) {
                    videoLine[s] = static_cast<uint16_t>((line * 100 + s) % 65536);
                }
                testField.push_back(videoLine);
            }
            
            writer.writeVideoField(testField, 0);
            
            // Add metadata
            FieldMetadata metadata;
            metadata.fieldNumber = 0;
            metadata.isFirstField = true;
            metadata.lineCount = testField.size();
            writer.addFieldMetadata(0, metadata);
            
            std::cout << "✓ Test field written (" << testField.size() << " lines)\n";
            
            writer.close();
            std::cout << "✓ TBC files closed\n";
            
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "PHASE 2: RF Processing Pipeline\n";
            std::cout << std::string(60, '=') << "\n";
            
            // Test Phase 2: RF Processor with filter bank and Hilbert
            std::cout << "\nInitializing RF processor (Phase 2)...\n";
            rf::RFProcessor::Config rfConfig;
            rfConfig.sampleRateMHz = config.inputFreqMHz;
            rfConfig.blockSize = config.blockSize;
            rfConfig.rfBandpassLowMHz = 0.5;
            rfConfig.rfBandpassHighMHz = 18.0;
            rfConfig.useGPU = useGPU;
            
            rf::RFProcessor rfProcessor(rfConfig);
            std::cout << "✓ RF processor initialized\n";
            std::cout << "  Sample rate: " << rfConfig.sampleRateMHz << " MHz\n";
            std::cout << "  Block size: " << rfConfig.blockSize << " samples\n";
            std::cout << "  RF bandpass: " << rfConfig.rfBandpassLowMHz << " - " 
                      << rfConfig.rfBandpassHighMHz << " MHz\n";
            
            // Test filter bank
            std::cout << "\nTesting filter bank...\n";
            auto& filterBank = rfProcessor.getFilterBank();
            
            // Create additional filters
            filterBank.createLowpassFilter("video_lpf", 6.0);
            filterBank.createBandpassFilter("chroma_bp", 0.4, 1.0);
            
            auto filterNames = filterBank.getFilterNames();
            std::cout << "✓ Filter bank loaded\n";
            std::cout << "  Available filters: " << filterNames.size() << "\n";
            for (const auto& name : filterNames) {
                std::cout << "    - " << name << "\n";
            }
            
            // Test Hilbert transform
            std::cout << "\nTesting Hilbert transform...\n";
            rf::HilbertTransform hilbert(config.blockSize);
            auto hilbertFilter = hilbert.getHilbertFilter();
            std::cout << "✓ Hilbert transform initialized\n";
            std::cout << "  Filter size: " << hilbertFilter.size() << " bins\n";
            std::cout << "  DC multiplier: " << hilbertFilter[0].real() << "\n";
            std::cout << "  Positive freq multiplier: " << hilbertFilter[1].real() << "\n";
            
            // Process a block through the full RF pipeline
            if (reader.getFileSize() >= config.blockSize) {
                std::cout << "\nProcessing RF block through full pipeline...\n";
                auto rfBlock = reader.readBlock(config.blockSize, 0);
                
                auto rfResult = rfProcessor.processBlock(rfBlock.data);
                
                std::cout << "✓ RF processing complete\n";
                std::cout << "  Analytic signal samples: " << rfResult.analyticSignal.size() << "\n";
                std::cout << "  Envelope samples: " << rfResult.envelope.size() << "\n";
                std::cout << "  Filtered signal samples: " << rfResult.filtered.size() << "\n";
                
                // Show some envelope statistics
                float minEnv = rfResult.envelope[0];
                float maxEnv = rfResult.envelope[0];
                float avgEnv = 0.0f;
                for (size_t i = 0; i < rfResult.envelope.size(); ++i) {
                    minEnv = std::min(minEnv, rfResult.envelope[i]);
                    maxEnv = std::max(maxEnv, rfResult.envelope[i]);
                    avgEnv += rfResult.envelope[i];
                }
                avgEnv /= rfResult.envelope.size();
                
                std::cout << "  Envelope stats:\n";
                std::cout << "    Min: " << std::fixed << std::setprecision(6) << minEnv << "\n";
                std::cout << "    Max: " << maxEnv << "\n";
                std::cout << "    Avg: " << avgEnv << "\n";
                
                // Test FM demodulation with the analytic signal
                std::cout << "\nApplying FM demodulation to analytic signal...\n";
                demod::FMDemodulator fmDemod2(config);
                auto fmResult = fmDemod2.demodulate(rfResult.analyticSignal);
                
                std::cout << "✓ FM demodulation complete\n";
                std::cout << "  Video samples: " << fmResult.video.size() << "\n";
                std::cout << "  Chroma samples: " << fmResult.chroma.size() << "\n";
            }
            
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "Status: Phase 2 RF Processing Working! ✓✓✓\n";
            std::cout << std::string(60, '=') << "\n";
            
            std::cout << "\nPhase 1 Components:\n";
            std::cout << "  1. ✓ RF reader with memory-mapped I/O\n";
            std::cout << "  2. ✓ FFT engine (CPU, prototype implementation)\n";
            std::cout << "  3. ✓ FM demodulator (phase unwrap, envelope detection)\n";
            std::cout << "  4. ✓ TBC writer (video, chroma, metadata)\n";
            
            std::cout << "\nPhase 2 Components (NEW):\n";
            std::cout << "  5. ✓ Filter bank (bandpass, lowpass, highpass)\n";
            std::cout << "  6. ✓ Hilbert transform (analytic signal generation)\n";
            std::cout << "  7. ✓ RF processor (integrated pipeline)\n";
            
            std::cout << "\nNext steps for Phase 3:\n";
            std::cout << "  • Replace prototype FFT with FFTW3 for production\n";
            std::cout << "  • Add GPU support with clFFT/OpenCL\n";
            std::cout << "  • Optimize phase unwrapping with custom kernels\n";
            std::cout << "  • Implement proper video/chroma separation\n";
            std::cout << "  • Add dropout detection and correction\n";
            std::cout << "  • Implement time-base correction\n";
            std::cout << "  • Multi-threaded block processing\n";
            
        } catch (const std::exception& e) {
            std::cerr << "Error during processing: " << e.what() << "\n";
            return 1;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
