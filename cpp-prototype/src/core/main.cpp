#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include "vhsdecode/types.hpp"
#include "vhsdecode/rf_reader.hpp"
#include "formats/format_base.hpp"
#include "formats/vhs_format.hpp"

using namespace vhsdecode;
using namespace vhsdecode::formats;

void printUsage(const char* progName) {
    std::cout << "VHS-Decode C++ Prototype v0.1.0\n";
    std::cout << "Usage: " << progName << " [options] <input.r40> <output.tbc>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --format <format>   Tape format (VHS, SVHS, Betamax) [default: VHS]\n";
    std::cout << "  --system <system>   TV system (NTSC, PAL, PAL-M) [default: NTSC]\n";
    std::cout << "  --threads <n>       Number of threads [default: 4]\n";
    std::cout << "  --gpu               Enable GPU acceleration\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << progName << " --format VHS --system NTSC input.r40 output.tbc\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string formatName = "VHS";
    std::string systemName = "NTSC";
    int threads = 4;
    bool useGPU = false;
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
        
        // Try to open RF file
        std::cout << "Opening RF file...\n";
        try {
            io::RFReader reader(inputFile, true);  // Use memory-mapped I/O
            
            std::cout << "✓ RF file opened successfully\n";
            std::cout << "  File size: " << reader.getFileSize() << " bytes\n";
            std::cout << "  Sample count: " << reader.getSampleCount(1) << " samples (uint8)\n";
            std::cout << "  Memory-mapped: " << (reader.isMemoryMapped() ? "Yes" : "No") << "\n";
            
            // Try reading first block
            if (reader.getFileSize() > 0) {
                auto block = reader.readBlock(config.blockSize, 0);
                std::cout << "  First block read: " << block.data.size() << " bytes\n";
                
                if (!block.data.empty()) {
                    // Show first few samples
                    std::cout << "  First samples: ";
                    for (size_t i = 0; i < std::min(size_t(10), block.data.size()); ++i) {
                        std::cout << static_cast<int>(block.data[i]) << " ";
                    }
                    std::cout << "...\n";
                }
            }
            
            std::cout << "\nStatus: RF reader working! ✓\n";
            std::cout << "Next steps:\n";
            std::cout << "  1. ✓ RF reader (DONE)\n";
            std::cout << "  2. Implement FFT engine\n";
            std::cout << "  3. Implement FM demodulator\n";
            std::cout << "  4. Implement TBC writer\n";
            std::cout << "  5. Connect full pipeline\n";
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to open RF file: " << e.what() << "\n";
            std::cerr << "(This is expected if the file doesn't exist - creating a test file...)\n";
            
            // Create a small test file for demonstration
            std::cout << "\nCreating test RF file with dummy data...\n";
            std::ofstream testFile(inputFile, std::ios::binary);
            if (testFile) {
                // Write 1MB of test data
                std::vector<uint8_t> testData(1024 * 1024);
                for (size_t i = 0; i < testData.size(); ++i) {
                    testData[i] = static_cast<uint8_t>(i % 256);
                }
                testFile.write(reinterpret_cast<const char*>(testData.data()), testData.size());
                testFile.close();
                
                std::cout << "✓ Test file created: " << inputFile << " (1 MB)\n";
                std::cout << "  Run again to test RF reader\n";
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
