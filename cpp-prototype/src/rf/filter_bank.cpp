#include "vhsdecode/filter_bank.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace vhsdecode {
namespace rf {

FilterBank::FilterBank(double sampleRateMHz, size_t blockSize)
    : sampleRateMHz_(sampleRateMHz)
    , blockSize_(blockSize)
{
}

FilterBank::~FilterBank() = default;

FilterBank::FilterBank(FilterBank&&) noexcept = default;
FilterBank& FilterBank::operator=(FilterBank&&) noexcept = default;

bool FilterBank::loadFilterFromCSV(const std::string& name, const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file) {
        return false;
    }
    
    // Parse CSV file (frequency, real, imag format)
    ComplexArray filter;
    filter.reserve(blockSize_ / 2 + 1);
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string token;
        std::vector<float> values;
        
        while (std::getline(iss, token, ',')) {
            try {
                values.push_back(std::stof(token));
            } catch (...) {
                continue;
            }
        }
        
        if (values.size() >= 3) {
            // frequency, real, imag
            filter.emplace_back(values[1], values[2]);
        } else if (values.size() >= 2) {
            // frequency, magnitude (real only)
            filter.emplace_back(values[1], 0.0f);
        }
    }
    
    if (filter.empty()) {
        return false;
    }
    
    // Resize to match block size if needed
    if (filter.size() < blockSize_ / 2 + 1) {
        filter.resize(blockSize_ / 2 + 1, std::complex<float>(0.0f, 0.0f));
    } else if (filter.size() > blockSize_ / 2 + 1) {
        filter.resize(blockSize_ / 2 + 1);
    }
    
    filters_[name] = std::move(filter);
    return true;
}

ComplexArray FilterBank::createIdealResponse(double lowCutoffMHz, 
                                             double highCutoffMHz,
                                             bool isComplex) {
    size_t numBins = blockSize_ / 2 + 1;
    ComplexArray response(numBins, std::complex<float>(0.0f, 0.0f));
    
    // Frequency resolution
    double freqResolution = sampleRateMHz_ / blockSize_;  // MHz per bin
    
    for (size_t i = 0; i < numBins; ++i) {
        double freqMHz = i * freqResolution;
        
        // Ideal brick-wall filter
        if (freqMHz >= lowCutoffMHz && freqMHz <= highCutoffMHz) {
            response[i] = std::complex<float>(1.0f, 0.0f);
        } else {
            response[i] = std::complex<float>(0.0f, 0.0f);
        }
    }
    
    return response;
}

bool FilterBank::createBandpassFilter(const std::string& name, 
                                     double lowCutoffMHz, 
                                     double highCutoffMHz) {
    if (lowCutoffMHz >= highCutoffMHz) {
        return false;
    }
    
    if (highCutoffMHz > sampleRateMHz_ / 2.0) {
        return false;  // Above Nyquist frequency
    }
    
    auto filter = createIdealResponse(lowCutoffMHz, highCutoffMHz);
    filters_[name] = std::move(filter);
    return true;
}

bool FilterBank::createLowpassFilter(const std::string& name, double cutoffMHz) {
    return createBandpassFilter(name, 0.0, cutoffMHz);
}

bool FilterBank::createHighpassFilter(const std::string& name, double cutoffMHz) {
    return createBandpassFilter(name, cutoffMHz, sampleRateMHz_ / 2.0);
}

bool FilterBank::createButterworthLPF(const std::string& name, double cutoffMHz, int order) {
    // Create frequency-domain Butterworth LPF matching scipy.signal.butter
    // H(f) = 1 / sqrt(1 + (f/fc)^(2*order))
    
    size_t numBins = blockSize_ / 2 + 1;
    ComplexArray response(numBins);
    
    // Frequency resolution in MHz
    double freqResolution = sampleRateMHz_ / static_cast<double>(blockSize_);
    
    for (size_t i = 0; i < numBins; ++i) {
        double freqMHz = static_cast<double>(i) * freqResolution;
        
        // Butterworth magnitude response: |H(f)| = 1 / sqrt(1 + (f/fc)^(2n))
        double normalized = freqMHz / cutoffMHz;
        double magnitude = 1.0 / std::sqrt(1.0 + std::pow(normalized, 2 * order));
        
        response[i] = std::complex<float>(static_cast<float>(magnitude), 0.0f);
    }
    
    filters_[name] = std::move(response);
    return true;
}

bool FilterBank::createButterworthHPF(const std::string& name, double cutoffMHz, int order) {
    // Create frequency-domain Butterworth HPF matching scipy.signal.butter
    // H(f) = (f/fc)^order / sqrt(1 + (f/fc)^(2*order))
    
    size_t numBins = blockSize_ / 2 + 1;
    ComplexArray response(numBins);
    
    // Frequency resolution in MHz
    double freqResolution = sampleRateMHz_ / static_cast<double>(blockSize_);
    
    for (size_t i = 0; i < numBins; ++i) {
        double freqMHz = static_cast<double>(i) * freqResolution;
        
        // Butterworth HPF magnitude response: |H(f)| = (f/fc)^n / sqrt(1 + (f/fc)^(2n))
        double normalized = freqMHz / cutoffMHz;
        double magnitude;
        if (normalized < 1e-10) {
            magnitude = 0.0;  // DC component is blocked
        } else {
            magnitude = std::pow(normalized, order) / std::sqrt(1.0 + std::pow(normalized, 2 * order));
        }
        
        response[i] = std::complex<float>(static_cast<float>(magnitude), 0.0f);
    }
    
    filters_[name] = std::move(response);
    return true;
}

bool FilterBank::createButterworthBPF(const std::string& name, double lowCutoffMHz, 
                                      double highCutoffMHz, int order) {
    // Create frequency-domain Butterworth bandpass filter
    // BPF = HPF(low) * LPF(high)
    
    size_t numBins = blockSize_ / 2 + 1;
    ComplexArray response(numBins);
    
    double freqResolution = sampleRateMHz_ / static_cast<double>(blockSize_);
    
    for (size_t i = 0; i < numBins; ++i) {
        double freqMHz = static_cast<double>(i) * freqResolution;
        
        // Butterworth HPF at low cutoff
        double normLow = freqMHz / lowCutoffMHz;
        double hpfMag = std::pow(normLow, order) / 
                        std::sqrt(1.0 + std::pow(normLow, 2 * order));
        
        // Butterworth LPF at high cutoff
        double normHigh = freqMHz / highCutoffMHz;
        double lpfMag = 1.0 / std::sqrt(1.0 + std::pow(normHigh, 2 * order));
        
        double magnitude = hpfMag * lpfMag;
        
        response[i] = std::complex<float>(static_cast<float>(magnitude), 0.0f);
    }
    
    filters_[name] = std::move(response);
    return true;
}

bool FilterBank::createCombinedFilter(const std::string& name,
                                       const std::string& filter1Name,
                                       const std::string& filter2Name) {
    // Create combined filter by element-wise multiplication of two existing filters
    // This allows creating bandpass from separate HPF * LPF with different orders
    
    if (!hasFilter(filter1Name)) {
        return false;
    }
    if (!hasFilter(filter2Name)) {
        return false;
    }
    
    const auto& filter1 = filters_[filter1Name];
    const auto& filter2 = filters_[filter2Name];
    
    if (filter1.size() != filter2.size()) {
        return false;
    }
    
    ComplexArray response(filter1.size());
    
    for (size_t i = 0; i < filter1.size(); ++i) {
        // Element-wise multiplication of complex filter coefficients
        response[i] = filter1[i] * filter2[i];
    }
    
    filters_[name] = std::move(response);
    return true;
}

const ComplexArray& FilterBank::getFilter(const std::string& name) const {
    auto it = filters_.find(name);
    if (it == filters_.end()) {
        throw std::runtime_error("Filter not found: " + name);
    }
    return it->second;
}

bool FilterBank::hasFilter(const std::string& name) const {
    return filters_.find(name) != filters_.end();
}

std::vector<std::string> FilterBank::getFilterNames() const {
    std::vector<std::string> names;
    names.reserve(filters_.size());
    
    for (const auto& pair : filters_) {
        names.push_back(pair.first);
    }
    
    return names;
}

} // namespace rf
} // namespace vhsdecode
