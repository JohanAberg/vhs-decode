#define _USE_MATH_DEFINES
#define NOMINMAX

#include <iostream>
#include <stdexcept>
#include <string>

#include "formats/format_base.hpp"
#include "vhsdecode/decoder_pipeline.hpp"
#include "vhsdecode/types.hpp"

using vhsdecode::core::DecoderObserver;
using vhsdecode::core::DecoderPipelineOptions;
using vhsdecode::core::DecoderPipelineResult;
using vhsdecode::FieldMetadata;

namespace {

void printUsage(const char* progName) {
    std::cout << "VHS-Decode C++ Prototype v0.1.0\n";
    std::cout << "Usage: " << progName << " [options] <input.r40> <output_basename>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --format <format>     Tape format (VHS, SVHS, Betamax) [default: VHS)\n";
    std::cout << "  --system <system>     TV system (NTSC, PAL, PAL-M) [default: NTSC]\n";
    std::cout << "  --threads <n>         Number of threads [default: 4]\n";
    std::cout << "  --length <n>          Number of frames to process (0 = all) [default: 0]\n";
    std::cout << "  --seek <n>            Skip to byte offset in input file [default: 0]\n";
    std::cout << "  --gpu                 Enable GPU acceleration\n";
    std::cout << "  --use-prototype-fft   Force slow prototype FFT implementation\n";
    std::cout << "  --no-align-vsync      Skip automatic alignment to detected vsync\n";
    std::cout << "  --help                Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << progName << " --format VHS --system NTSC input.r40 output\n";
}

class ConsoleObserver : public DecoderObserver {
public:
    void onLogMessage(const std::string& message) override {
        std::cout << message << std::endl;
    }

    void onProgress(size_t processed, size_t total) override {
        if (total == 0) {
            return;
        }
        const size_t percent = (processed * 100) / total;
        std::cout << "[CLI] Progress: " << percent << "% (" << processed << "/" << total
                  << " blocks)\r" << std::flush;
    }

    void onFieldWritten(const FieldMetadata& metadata) override {
        std::cout << "[CLI] Field " << metadata.seqNo << " written at fileLoc="
                  << metadata.fileLoc << std::endl;
    }
};

} // namespace

int main(int argc, char* argv[]) {
    std::string formatName = "VHS";
    std::string systemName = "NTSC";
    DecoderPipelineOptions options;
    ConsoleObserver observer;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--format" && i + 1 < argc) {
            formatName = argv[++i];
        } else if (arg == "--system" && i + 1 < argc) {
            systemName = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            options.threadCount = std::stoi(argv[++i]);
        } else if (arg == "--length" && i + 1 < argc) {
            options.lengthFrames = std::stoi(argv[++i]);
        } else if (arg == "--seek" && i + 1 < argc) {
            options.seekOffset = std::stoull(argv[++i]);
        } else if (arg == "--gpu") {
            options.useGPU = true;
        } else if (arg == "--use-prototype-fft") {
            options.usePrototypeFFT = true;
        } else if (arg == "--no-align-vsync") {
            options.alignToFirstField = false;
        } else if (!arg.empty() && arg[0] != '-') {
            if (options.inputPath.empty()) {
                options.inputPath = arg;
            } else if (options.outputBasename.empty()) {
                options.outputBasename = arg;
            } else {
                std::cerr << "Too many positional arguments provided" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (options.inputPath.empty() || options.outputBasename.empty()) {
        std::cerr << "Error: Input file and output basename are required\n\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        options.format = vhsdecode::formats::formatFromString(formatName);
        options.system = vhsdecode::formats::systemFromString(systemName);

        DecoderPipelineResult result = vhsdecode::core::runDecoderPipeline(options, &observer);
        std::cout << "\nSummary: " << result.writtenFields << " fields, "
                  << result.processedBlocks << "/" << result.totalBlocks
                  << " blocks processed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
