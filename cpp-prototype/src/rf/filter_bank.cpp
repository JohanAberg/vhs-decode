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
