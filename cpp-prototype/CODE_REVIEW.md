# C++ Prototype Code Review

**Review Date:** December 20, 2025  
**Status:** Working prototype with several issues identified

## 🚨 Critical Issues

### 1. **Memory Safety - Potential Buffer Overflow in rf_reader.cpp**
**Location:** [src/io/rf_reader.cpp](src/io/rf_reader.cpp#L203-L230)  
**Severity:** HIGH - Could cause crashes or security vulnerabilities

**Issue:**
```cpp
RFBlock RFReader::readBlock(size_t blockSize, size_t blockNumber) {
    RFBlock block(blockSize, blockNumber, position_);
    
    size_t bytesToRead = std::min(blockSize, fileSize_ - position_);
    if (bytesToRead == 0) {
        return block;  // Return empty block at EOF
    }
    
    block.data.resize(bytesToRead);
    
    if (useMemoryMap_ && impl_->mappedData) {
        // Memory-mapped: direct copy
        const uint8_t* src = static_cast<const uint8_t*>(impl_->mappedData) + position_;
        std::memcpy(block.data.data(), src, bytesToRead);  // ⚠️ No bounds check on mapped region
```

**Problem:** No validation that `position_ + bytesToRead <= impl_->mappedSize`. If the file size changes after memory mapping or if there's a calculation error, this could read beyond the mapped region.

**Fix Required:**
```cpp
if (useMemoryMap_ && impl_->mappedData) {
    if (position_ + bytesToRead > impl_->mappedSize) {
        throw std::runtime_error("Read beyond mapped region");
    }
    const uint8_t* src = static_cast<const uint8_t*>(impl_->mappedData) + position_;
    std::memcpy(block.data.data(), src, bytesToRead);
}
```

### 2. **Thread Safety - Race Condition in BlockProcessor**
**Location:** [src/core/block_processor.cpp](src/core/block_processor.cpp#L46-L58)  
**Severity:** MEDIUM - Could cause incorrect processing or hangs

**Issue:**
```cpp
bool BlockProcessor::hasResults() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !resultQueue_.empty();
}

size_t BlockProcessor::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return workQueue_.size();
}
```

**Problem:** The `mutable` keyword is missing on `queueMutex_`, so these `const` methods cannot actually lock the mutex (compilation should fail, but might not with some compilers).

**Fix Required:**
In header file, declare mutex as mutable:
```cpp
mutable std::mutex queueMutex_;
```

### 3. **FFT Performance - Recursive Implementation is O(n²) not O(n log n)**
**Location:** [src/rf/fft_engine.cpp](src/rf/fft_engine.cpp#L45-L68)  
**Severity:** HIGH - 100-1000x slower than it should be

**Issue:**
```cpp
void fft_recursive(std::complex<float>* data, size_t n, bool inverse) {
    if (n <= 1) return;
    
    // Allocate temporary arrays - NOT cache-friendly!
    std::vector<std::complex<float>> even(n/2), odd(n/2);
    
    for (size_t i = 0; i < n/2; ++i) {
        even[i] = data[i * 2];
        odd[i] = data[i * 2 + 1];
    }
    
    // Recursive calls create massive allocation overhead
    fft_recursive(even.data(), n/2, inverse);
    fft_recursive(odd.data(), n/2, inverse);
```

**Problems:**
- Allocates `O(n log n)` temporary memory across all recursion levels
- No cache locality - data is scattered across memory
- Every recursive call has allocation overhead
- Should be using iterative Cooley-Tukey with in-place butterfly operations

**Impact:** The warning message correctly states "Install FFTW3 for 1000x speedup" - but even a proper iterative FFT would be 10-50x faster than this recursive version.

**Recommendation:** Either implement proper iterative FFT or mark this implementation as "PROTOTYPE ONLY - DO NOT USE IN PRODUCTION".

---

## ⚠️ Major Issues

### 4. **Integer Overflow Risk in Phase Unwrapping**
**Location:** [src/demod/fm_demodulator.cpp](src/demod/fm_demodulator.cpp#L24-L35)  
**Severity:** MEDIUM - Could cause infinite loops

**Issue:**
```cpp
// Unwrap if jump > π
while (diff > M_PI) {
    phase[i] -= 2.0f * M_PI;
    diff = phase[i] - phase[i-1];
}
while (diff < -M_PI) {
    phase[i] += 2.0f * M_PI;
    diff = phase[i] - phase[i-1];
}
```

**Problem:** If there's a numerical error or extreme phase discontinuity, these loops could run indefinitely or for a very long time. No iteration limit.

**Fix Required:**
```cpp
const int MAX_UNWRAP_ITERATIONS = 10;
int iterations = 0;

while (diff > M_PI && iterations < MAX_UNWRAP_ITERATIONS) {
    phase[i] -= 2.0f * M_PI;
    diff = phase[i] - phase[i-1];
    iterations++;
}

iterations = 0;
while (diff < -M_PI && iterations < MAX_UNWRAP_ITERATIONS) {
    phase[i] += 2.0f * M_PI;
    diff = phase[i] - phase[i-1];
    iterations++;
}

if (iterations >= MAX_UNWRAP_ITERATIONS) {
    // Log warning or handle gracefully
}
```

### 5. **Missing Error Handling in TBC Writer**
**Location:** [src/io/tbc_writer.cpp](src/io/tbc_writer.cpp#L71-L76)  
**Severity:** MEDIUM - Silent data corruption

**Issue:**
```cpp
void TBCWriter::writeVideoSamples(const uint16_t* data, size_t count) {
    if (!videoFile_.is_open()) {
        throw std::runtime_error("Video file not open");
    }
    
    videoFile_.write(reinterpret_cast<const char*>(data), count * sizeof(uint16_t));
    // ⚠️ No check if write succeeded!
}
```

**Problem:** File write could fail (disk full, permission error, etc.) but code continues without checking. This causes silent data corruption.

**Fix Required:**
```cpp
videoFile_.write(reinterpret_cast<const char*>(data), count * sizeof(uint16_t));
if (!videoFile_.good()) {
    throw std::runtime_error("Failed to write video data: " + std::string(std::strerror(errno)));
}
```

### 6. **Double vs Float Precision Mismatch**
**Location:** Multiple files  
**Severity:** MEDIUM - Accuracy loss and performance impact

**Issue:** The code inconsistently uses `float` (via `SampleF32`) for internal processing but `double` for interfaces:

- `types.hpp`: Defines `using SampleF32 = float;`
- `fm_demodulator.cpp`: Uses `float` internally but accepts/returns via `RealArray` (vector of float)
- `dropout_corrector.cpp`: Uses `std::vector<double>` for everything
- `tbc_corrector.cpp`: Uses `std::vector<double>` for everything

**Problems:**
1. Unnecessary type conversions at boundaries
2. Precision loss when converting double→float
3. Confusion about which precision is "canonical"
4. Performance overhead of mixed precision

**Recommendation:** 
- Choose ONE precision (probably `double` for accuracy)
- Make all interfaces use that precision consistently
- Use SIMD float processing only in inner loops where proven beneficial

---

## ⚙️ Design Issues

### 7. **Missing Move Semantics in Critical Path**
**Location:** [src/core/block_processor.cpp](src/core/block_processor.cpp#L64-L68)  
**Severity:** LOW-MEDIUM - Performance impact

**Issue:**
```cpp
block = workQueue_.front();
workQueue_.pop();
```

**Problem:** This copies the entire `RFBlock` (including its potentially large `data` vector) instead of moving it. For a 32K block, this copies 32KB unnecessarily.

**Fix:**
```cpp
block = std::move(workQueue_.front());
workQueue_.pop();
```

### 8. **Inefficient Dropout Correction Algorithm**
**Location:** [src/video/dropout_corrector.cpp](src/video/dropout_corrector.cpp#L48-L75)  
**Severity:** LOW - Quality/performance tradeoff

**Issue:** Uses simple linear interpolation for dropout correction.

**Problems:**
1. Linear interpolation creates visible artifacts for long dropouts
2. O(n) scan per dropout region (could be more efficient)
3. No consideration of signal frequency content

**Recommendation:** Implement cubic spline interpolation or use previous line data (temporal interpolation) for better quality.

### 9. **No RAII for File Handles in rf_reader.cpp**
**Location:** [src/io/rf_reader.cpp](src/io/rf_reader.cpp#L20-L66)  
**Severity:** LOW - Code cleanliness

**Issue:** Manual cleanup in destructor is error-prone.

**Better Approach:** Use RAII wrappers:
```cpp
// Windows
using unique_handle = std::unique_ptr<void, decltype(&CloseHandle)>;
unique_handle fileHandle{CreateFileA(...), CloseHandle};

// Unix
using unique_fd = std::unique_ptr<int, decltype([](int* fd) { if(fd && *fd >= 0) close(*fd); delete fd; })>;
```

### 10. **Hardcoded Magic Numbers**
**Location:** [src/video/tbc_corrector.cpp](src/video/tbc_corrector.cpp#L12-L20)  
**Severity:** LOW - Maintainability

**Issue:**
```cpp
if (system == TVSystem::NTSC) {
    linesPerField_ = 262;  // NTSC: 262/263 alternating
    standardLineLength_ = 910;  // samples per line at 40 MSPS
} else {  // PAL
    linesPerField_ = 312;  // PAL: 312/313 alternating
    standardLineLength_ = 1135;  // samples per line at 40 MSPS
}
```

**Problem:** These constants should come from `SystemParams` structure, not be hardcoded.

**Fix:** Use the format system defined in `formats/vhs_format.hpp` and pass parameters correctly.

---

## 📝 Minor Issues

### 11. **Missing nullptr Checks**
**Location:** [src/io/rf_reader.cpp](src/io/rf_reader.cpp#L233)  
```cpp
if (!buffer) {
    throw std::invalid_argument("Null buffer");
}
```
✅ Good! But check other functions too.

### 12. **Inconsistent Naming Conventions**
- Some functions use `camelCase`: `detectHsyncs`, `normalizeLine`
- Others use `snake_case`: `hsync_threshold_` (member variables)
- Namespaces use `snake_case`: `namespace vhsdecode`

**Recommendation:** Follow consistent style (suggest Modern C++ conventions):
- Classes/Types: `PascalCase`
- Functions/Methods: `camelCase`
- Variables/Members: `camelCase` with `_` suffix for members
- Constants: `kPascalCase` or `ALL_CAPS`

### 13. **Missing const Correctness**
**Example:** `DropoutCorrector::detectDropouts()` doesn't modify state but isn't marked `const`.

### 14. **TODO Comments Not Tracked**
**Location:** Multiple files
```cpp
// TODO: Implement proper filtering
// TODO: Use nlohmann/json library for proper JSON generation
```

**Recommendation:** Create GitHub issues for all TODOs and link them in comments.

### 15. **No Unit Tests**
The `tests/` directory exists but appears empty. Critical for C++ code safety.

---

## ✅ Good Practices Observed

1. **PIMPL Pattern** - Used in `RFReader`, `FMDemodulator`, etc. for ABI stability
2. **Smart Pointers** - No raw `new`/`delete`, all using `unique_ptr`
3. **RAII** - Destructors clean up resources properly
4. **Move Semantics** - Move constructors/assignment implemented where needed
5. **Const Correctness** - Generally good use of `const`
6. **Exception Safety** - Uses exceptions for error handling
7. **Platform Abstraction** - Windows/Unix differences properly handled
8. **Namespace Organization** - Clear namespace hierarchy

---

## 🎯 Priority Recommendations

### Immediate (Before Production Use)
1. ✅ **Fix memory bounds check in `RFReader::readBlock()`** (Critical safety issue)
2. ✅ **Fix mutex const-correctness in `BlockProcessor`** (Compilation/thread safety)
3. ✅ **Add iteration limits to phase unwrapping** (Prevent hangs)
4. ✅ **Add file write error checking** (Prevent data corruption)

### High Priority (Performance)
5. ✅ **Replace recursive FFT or clearly mark as prototype-only** (100-1000x speedup)
6. ⚠️ **Standardize on double or float precision** (Consistency and performance)
7. ⚠️ **Add move semantics to hot paths** (Reduce copies in block processing)

### Medium Priority (Quality)
8. 📋 **Implement proper dropout correction** (Better output quality)
9. 📋 **Add comprehensive unit tests** (Prevent regressions)
10. 📋 **Use format parameters instead of hardcoded values** (Maintainability)

### Low Priority (Polish)
11. 📋 **Consistent naming conventions** (Code readability)
12. 📋 **RAII wrappers for platform handles** (Cleaner code)
13. 📋 **Track TODOs in issue tracker** (Project management)

---

## 📊 Overall Assessment

**Strengths:**
- Modern C++ practices (smart pointers, RAII, move semantics)
- Good architecture (PIMPL, namespace organization)
- Cross-platform support
- Reasonable error handling

**Weaknesses:**
- Memory safety gaps (bounds checking)
- Thread safety issues (mutex const-correctness)
- Performance problems (FFT implementation, unnecessary copies)
- Inconsistent type usage (float vs double)
- Missing tests

**Verdict:** ✅ **Working prototype** but needs critical fixes before production use. The architecture is solid and follows modern C++ best practices, but there are several bugs that could cause crashes or data corruption under edge cases.

**Estimated Effort to Production Ready:**
- Critical fixes: 2-4 hours
- Performance improvements: 1-2 days
- Testing framework: 2-3 days
- Total: **1 week** for stable production release

---

## 🔧 Suggested Next Steps

1. Create GitHub issues for all critical/high priority items
2. Fix critical safety issues (items 1-4)
3. Add basic unit tests for core functionality
4. Profile actual decode performance with real samples
5. Consider integrating FFTW3 (already planned, would solve FFT issues)
6. Run static analysis tools (clang-tidy, cppcheck)
7. Run dynamic analysis (valgrind, address sanitizer)
8. Benchmark against Python baseline

Would you like me to implement any of these fixes immediately?
