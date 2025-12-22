/**
 * @file generate_test_rf.cpp
 * @brief Generate synthetic VHS RF test signal for benchmarking and testing
 * 
 * Creates a realistic VHS RF signal with:
 * - FM modulated video with sync pulses
 * - Color subcarrier
 * - Noise
 * - Multiple fields/frames
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>
#include <cstdint>

constexpr double SAMPLE_RATE = 40e6;  // 40 MSPS
constexpr double PI = 3.14159265358979323846;

// VHS PAL RF parameters (from format_defs)
constexpr double IRE_0_HZ = 4.1e6;     // 0 IRE frequency
constexpr double HZ_PER_IRE = 7000.0;  // Hz per IRE
constexpr double VSYNC_IRE = -42.857;  // Sync tip level

// Calculate frequencies
constexpr double SYNC_FREQ = IRE_0_HZ + (VSYNC_IRE * HZ_PER_IRE);  // ~3.8 MHz
constexpr double BLACK_FREQ = IRE_0_HZ;                             // 4.1 MHz  
constexpr double WHITE_FREQ = IRE_0_HZ + (100.0 * HZ_PER_IRE);     // ~4.8 MHz

// PAL timing (625 lines, 25 fps)
constexpr double LINE_DURATION = 64e-6;    // 64 microseconds
constexpr size_t SAMPLES_PER_LINE = static_cast<size_t>(SAMPLE_RATE * LINE_DURATION); // 2560
constexpr size_t LINES_PER_FIELD = 312;
constexpr size_t SAMPLES_PER_FIELD = SAMPLES_PER_LINE * LINES_PER_FIELD;

// Sync pulse timing
constexpr double HSYNC_DURATION = 4.7e-6;  // Horizontal sync pulse
constexpr size_t HSYNC_SAMPLES = static_cast<size_t>(SAMPLE_RATE * HSYNC_DURATION);

/**
 * Generate a single video line with sync pulse
 * @param lineNum Line number in field (0-311)
 * @param phaseOffset Phase offset for continuous signal
 * @return Vector of RF samples for one line
 */
std::vector<uint8_t> generateVideoLine(size_t lineNum, double& phaseOffset) {
    std::vector<uint8_t> line(SAMPLES_PER_LINE);
    
    // Add some pseudo-random content for variety
    static std::mt19937 rng(lineNum);
    std::uniform_real_distribution<double> contentDist(0.3, 0.7);
    double contentIRE = contentDist(rng) * 100.0; // 30-70% of white level
    
    for (size_t i = 0; i < SAMPLES_PER_LINE; ++i) {
        double freq;
        
        // Horizontal sync pulse at start of line
        if (i < HSYNC_SAMPLES) {
            freq = SYNC_FREQ;
        }
        // Color burst after sync (for testing chroma path)
        else if (i >= HSYNC_SAMPLES && i < HSYNC_SAMPLES + 200) {
            // Modulate with 4.43 MHz burst
            double burstPhase = 2.0 * PI * 4.43e6 * (i / SAMPLE_RATE);
            double burstAmp = 0.3 * std::cos(burstPhase);
            freq = BLACK_FREQ + burstAmp * HZ_PER_IRE * 20.0; // ±20 IRE burst
        }
        // Active video content
        else {
            freq = BLACK_FREQ + contentIRE * HZ_PER_IRE;
            
            // Add some horizontal variation (test pattern bars)
            double barPhase = (i - HSYNC_SAMPLES) / static_cast<double>(SAMPLES_PER_LINE - HSYNC_SAMPLES);
            double barPattern = 0.3 * std::sin(barPhase * 8.0 * PI); // 4 cycles per line
            freq += barPattern * HZ_PER_IRE * 30.0;
        }
        
        // FM modulate: integrate frequency to get phase
        phaseOffset += 2.0 * PI * freq / SAMPLE_RATE;
        
        // Generate RF carrier (modulated by phase)
        double rfSignal = std::sin(phaseOffset);
        
        // Add small amount of noise
        static std::uniform_real_distribution<double> noiseDist(-0.05, 0.05);
        rfSignal += noiseDist(rng);
        
        // Convert to 8-bit unsigned (centered at 127)
        int value = static_cast<int>(127.5 + rfSignal * 95.0);
        line[i] = static_cast<uint8_t>(std::max(0, std::min(255, value)));
    }
    
    return line;
}

/**
 * Generate a complete field with vertical blanking
 */
std::vector<uint8_t> generateField(size_t fieldNum) {
    std::vector<uint8_t> field;
    field.reserve(SAMPLES_PER_FIELD);
    
    double phaseOffset = 0.0;
    
    for (size_t lineNum = 0; lineNum < LINES_PER_FIELD; ++lineNum) {
        auto line = generateVideoLine(lineNum, phaseOffset);
        field.insert(field.end(), line.begin(), line.end());
    }
    
    return field;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <output.u8> <num_fields>\n";
        std::cerr << "Example: " << argv[0] << " test.u8 10\n";
        return 1;
    }
    
    const std::string outputFile = argv[1];
    const int numFields = std::stoi(argv[2]);
    
    std::cout << "Generating synthetic VHS PAL RF signal...\n";
    std::cout << "  Sample rate: " << SAMPLE_RATE / 1e6 << " MHz\n";
    std::cout << "  Lines per field: " << LINES_PER_FIELD << "\n";
    std::cout << "  Samples per line: " << SAMPLES_PER_LINE << "\n";
    std::cout << "  Fields: " << numFields << "\n";
    std::cout << "  Total size: " << (SAMPLES_PER_FIELD * numFields) / 1024.0 / 1024.0 << " MB\n";
    
    std::ofstream out(outputFile, std::ios::binary);
    if (!out) {
        std::cerr << "Error: Could not open output file: " << outputFile << "\n";
        return 1;
    }
    
    for (int fieldNum = 0; fieldNum < numFields; ++fieldNum) {
        auto field = generateField(fieldNum);
        out.write(reinterpret_cast<const char*>(field.data()), field.size());
        
        if (fieldNum % 10 == 0) {
            std::cout << "  Generated field " << fieldNum << "/" << numFields << "\r" << std::flush;
        }
    }
    
    std::cout << "\nDone! Written " << outputFile << "\n";
    
    return 0;
}
