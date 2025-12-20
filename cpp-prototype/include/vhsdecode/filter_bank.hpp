#pragma once

#include "vhsdecode/types.hpp"
#include <string>
#include <map>
#include <memory>

namespace vhsdecode {
namespace rf {

/**
 * @brief Filter bank for frequency-domain filtering
 * 
 * Manages pre-computed frequency-domain filter coefficients for RF processing.
 * Filters are loaded from CSV files (Python format compatibility) or computed.
 */
class FilterBank {
public:
    /**
     * @brief Filter specification
     */
    struct FilterSpec {
        std::string name;
        double lowCutoffMHz;
        double highCutoffMHz;
        double sampleRateMHz;
        size_t length;
        
        FilterSpec() : lowCutoffMHz(0), highCutoffMHz(0), sampleRateMHz(40.0), length(0) {}
    };
    
    /**
     * @brief Create filter bank
     * @param sampleRateMHz Sample rate in MHz (default 40 MHz)
     * @param blockSize FFT block size
     */
    explicit FilterBank(double sampleRateMHz = 40.0, size_t blockSize = 32768);
    
    /**
     * @brief Destructor
     */
    ~FilterBank();
    
    // Disable copy, allow move
    FilterBank(const FilterBank&) = delete;
    FilterBank& operator=(const FilterBank&) = delete;
    FilterBank(FilterBank&&) noexcept;
    FilterBank& operator=(FilterBank&&) noexcept;
    
    /**
     * @brief Load filter from CSV file (Python format)
     * @param name Filter name/identifier
     * @param csvPath Path to CSV file with frequency response
     * @return true if loaded successfully
     */
    bool loadFilterFromCSV(const std::string& name, const std::string& csvPath);
    
    /**
     * @brief Create bandpass filter
     * @param name Filter name/identifier
     * @param lowCutoffMHz Low cutoff frequency in MHz
     * @param highCutoffMHz High cutoff frequency in MHz
     * @return true if created successfully
     */
    bool createBandpassFilter(const std::string& name, 
                             double lowCutoffMHz, 
                             double highCutoffMHz);
    
    /**
     * @brief Create lowpass filter
     * @param name Filter name/identifier
     * @param cutoffMHz Cutoff frequency in MHz
     * @return true if created successfully
     */
    bool createLowpassFilter(const std::string& name, double cutoffMHz);
    
    /**
     * @brief Create highpass filter
     * @param name Filter name/identifier
     * @param cutoffMHz Cutoff frequency in MHz
     * @return true if created successfully
     */
    bool createHighpassFilter(const std::string& name, double cutoffMHz);
    
    /**
     * @brief Get filter coefficients
     * @param name Filter name
     * @return Filter coefficients (frequency domain)
     */
    const ComplexArray& getFilter(const std::string& name) const;
    
    /**
     * @brief Check if filter exists
     */
    bool hasFilter(const std::string& name) const;
    
    /**
     * @brief Get sample rate
     */
    double getSampleRate() const { return sampleRateMHz_; }
    
    /**
     * @brief Get block size
     */
    size_t getBlockSize() const { return blockSize_; }
    
    /**
     * @brief Get list of available filters
     */
    std::vector<std::string> getFilterNames() const;

private:
    double sampleRateMHz_;
    size_t blockSize_;
    
    std::map<std::string, ComplexArray> filters_;
    
    // Helper to create ideal frequency response
    ComplexArray createIdealResponse(double lowCutoffMHz, 
                                     double highCutoffMHz,
                                     bool isComplex = false);
};

} // namespace rf
} // namespace vhsdecode
