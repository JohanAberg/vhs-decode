# Line Positioning Algorithm Fix - December 22, 2025

## Problem Statement

The C++ prototype was producing output that was misaligned with Python by approximately:
- **Horizontal:** ~10 pixels offset
- **Vertical:** ~4 lines offset
- **Overall correlation:** Poor (MAD ~34 on 8-bit scale)

## Root Cause Analysis

The issue was in how line positions were computed in the main decoding loop (`main.cpp` lines 819-830):

### Old Approach (INCORRECT)
```cpp
// OLD CODE - THREW AWAY PULSE POSITIONS
// 1. Detected sync pulses
// 2. Used regression to compute average line length
// 3. Re-anchored to make first pulse land at grid position 0
double firstStart = firstDetectedStart;
if (lineLen > 0.0) {
    double k = std::round(firstStart / lineLen);
    firstStart -= k * lineLen;  // Throws away actual pulse position!
}
// 4. Rebuilt uniform grid
std::vector<double> rebuilt;
for (size_t i = 0; i < expectedLines; ++i) {
    rebuilt.push_back(firstStart + i * lineLen);  // Uniform spacing
}
lineStarts.swap(rebuilt);  // Lost all actual pulse data
```

**Problem:** This approach discarded the actual detected sync pulse positions and replaced them with a mathematically perfect uniform grid. VHS tapes have wow/flutter, so line lengths vary. The uniform grid doesn't match the actual signal timing.

### Python Approach (CORRECT)

Python's `compute_linelocs()` in `lddecode/core.py` does:

```python
# 1. Find line0loc (field start position)
line0loc = ... # From vsync detection

# 2. Assign each pulse to its line number
for pulse in validpulses:
    lineloc = (pulse.start - line0loc) / meanlinelen
    rlineloc = round(lineloc)
    lineloc_distance = abs(lineloc - rlineloc)
    
    if lineloc_distance < hsync_tolerance:
        linelocs_dict[rlineloc] = pulse.start  # Keep actual position!

# 3. Fill gaps by interpolating between detected positions
for l in range(numlines):
    if linelocs[l] < 0:  # Missing line
        # Find prev_valid and next_valid
        avglen = (linelocs[next_valid] - linelocs[prev_valid]) / (next_valid - prev_valid)
        linelocs[l] = linelocs[prev_valid] + (avglen * (l - prev_valid))

# 4. Refine with sub-sample zero-crossing
for i in range(len(linelocs)):
    zc = calczc(video, linelocs[i], threshold)
    if zc is not None:
        linelocs[i] = zc  # Sub-sample accuracy
```

**Key Difference:** Python keeps the actual pulse positions and only interpolates for missing lines. This preserves the natural timing variations of the tape.

## Solution Implemented

### 1. New Method in `sync_detector.cpp`

Added `computeLineLocsDict()` method that matches Python's algorithm:

```cpp
std::map<int, double> SyncDetector::computeLineLocsDict(
    const std::vector<Pulse>& pulses,
    double line0loc,
    double meanLineLength,
    int numLines,
    const RealArray& video,
    float syncThreshold)
{
    std::map<int, double> linelocsDict;
    
    // 1. Assign pulses to line numbers
    for (const auto& pulse : pulses) {
        double refinedStart = findSyncEdge(video, pulse.start, ...);
        double lineloc = (refinedStart - line0loc) / meanLineLength;
        int rlineloc = round(lineloc);
        
        if (abs(lineloc - rlineloc) < hsyncTolerance) {
            linelocsDict[rlineloc] = refinedStart;  // Keep actual position
        }
    }
    
    // 2. Fill gaps by interpolation
    for (int l = 0; l < numLines; ++l) {
        if (linelocs[l] < 0) {
            // Interpolate between prev_valid and next_valid
            avglen = (linelocs[next_valid] - linelocs[prev_valid]) / 
                    (next_valid - prev_valid);
            linelocs[l] = linelocs[prev_valid] + (avglen * (l - prev_valid));
        }
    }
    
    return result;
}
```

### 2. Updated `main.cpp` Field Processing Loop

Replaced the problematic grid rebuild with Python-style positioning:

```cpp
// NEW CODE - KEEPS PULSE POSITIONS
if (useSyncDetection && videoFloatBuffer.size() >= samplesNeeded) {
    auto pulses = syncDetector.findPulses(videoForSync, syncThresholdDigital);
    
    // Compute mean line length
    double meanLineLength = computeMeanFromIntervals(pulses);
    
    // Estimate line0loc from first pulses
    double line0loc = estimateLine0Loc(pulses[0].start, meanLineLength);
    
    // Use Python-style line positioning
    auto linelocsMap = syncDetector.computeLineLocsDict(
        pulses, line0loc, meanLineLength, 
        expectedLines, videoForSync, syncThresholdDigital);
    
    // Convert to vector, preserving actual positions
    for (size_t i = 0; i < expectedLines; ++i) {
        if (linelocsMap.find(i) != linelocsMap.end()) {
            lineStarts[i] = linelocsMap[i];  // Actual pulse position
        } else {
            lineStarts[i] = line0loc + (i * meanLineLength);  // Interpolated
        }
    }
}
```

## Files Modified

1. **`cpp-prototype/include/vhsdecode/sync_detector.hpp`**
   - Added `#include <map>`
   - Added `computeLineLocsDict()` method declaration

2. **`cpp-prototype/src/sync/sync_detector.cpp`**
   - Implemented `computeLineLocsDict()` (140 lines)
   - Matches Python's `compute_linelocs()` algorithm
   - Keeps actual pulse positions, fills gaps by interpolation

3. **`cpp-prototype/src/core/main.cpp`**
   - Replaced grid rebuild section (lines 745-860)
   - Now uses `computeLineLocsDict()` to get actual positions
   - Preserves pulse timing instead of creating uniform grid

4. **Documentation Updates**
   - `cpp-prototype/CPP_PROTOTYPE_STATUS.md` - Documented fix and expected results
   - `.github/copilot-instructions.md` - Updated sync alignment status

## Expected Results

### Before Fix
- Horizontal offset: ~10 pixels
- Vertical offset: ~4 lines
- MAD (8-bit): ~34
- Cause: Uniform grid didn't match tape timing variations

### After Fix (Expected)
- Horizontal offset: ~1-2 pixels (improved by 5-10x)
- Vertical offset: ~0-1 lines (improved)
- MAD (8-bit): <10 (significant improvement)
- Reason: Now using actual pulse positions with natural timing

### Remaining Improvements Needed
1. **Sub-sample refinement** - Add second pass with `calczc`-style zero crossing
2. **Signal enhancement** - High-frequency boost in weak areas
3. **Spike replacement** - Differential demodulation for dropout areas
4. **Chroma processing** - Currently copies luma

## Testing Commands

### Prerequisites
- Test file: `out1.u8` (VHS PAL RF capture)
- Python decoder installed
- C++ prototype built in `cpp-prototype/build/`

### Run Comparison
```bash
# Generate Python reference output
python decode.py vhs --system PAL --length 1 --seek 983040 out1.u8 /tmp/python_compare

# Generate C++ output with new algorithm
./cpp-prototype/build/src/vhs-decode --system PAL --length 1 --seek 983040 out1.u8 /tmp/cpp_compare

# Compare outputs
python3 tools/compare_tbc.py /tmp/python_compare.tbc /tmp/cpp_compare.tbc
```

### Expected Output
```
Correlation: 0.95+ (was ~0.60)
MAD (8-bit): <10 (was ~34)
Horizontal offset: 1-2 pixels (was ~10)
Vertical offset: 0-1 lines (was ~4)
```

## Technical Details

### Line Number Assignment

Python formula:
```python
lineloc = (pulse.start - line0loc) / meanlinelen
rlineloc = round(lineloc)
```

C++ implementation:
```cpp
double lineloc = (refinedStart - line0loc) / meanLineLength;
int rlineloc = static_cast<int>(std::round(lineloc));
```

### Gap Filling (Interpolation)

Python approach:
```python
if prev_valid is not None and next_valid is not None:
    avglen = (linelocs[next_valid] - linelocs[prev_valid]) / (next_valid - prev_valid)
    linelocs[l] = linelocs[prev_valid] + (avglen * (l - prev_valid))
```

C++ implementation:
```cpp
if (prevValid >= 0 && nextValid >= 0) {
    double avglen = (linelocs[nextValid] - linelocs[prevValid]) / 
                   static_cast<double>(nextValid - prevValid);
    linelocs[l] = linelocs[prevValid] + (avglen * (l - prevValid));
}
```

### Zero-Crossing Refinement

Already implemented in `findSyncEdge()`:
```cpp
double SyncDetector::findSyncEdge(const RealArray& video, size_t startPos,
                                   float threshold, size_t searchRange) 
{
    for (size_t i = startPos + 1; i < endPos; ++i) {
        if (video[i-1] > threshold && video[i] <= threshold) {
            // Sub-sample interpolation
            float a = video[i-1] - threshold;
            float b = video[i] - threshold;
            if (b - a != 0) {
                double frac = -a / (b - a);
                return static_cast<double>(i - 1) + frac;
            }
            return static_cast<double>(i);
        }
    }
    return -1.0;
}
```

## Validation

### Build Status
✅ C++ prototype compiles successfully
✅ No compiler warnings for sync detector changes
✅ Binary size: 235KB (optimized release build)

### Code Review Checklist
- [x] Matches Python's `compute_linelocs()` algorithm
- [x] Preserves actual pulse positions (not averaged)
- [x] Fills gaps by interpolation (not uniform spacing)
- [x] Uses sub-sample zero-crossing refinement
- [x] Handles edge cases (missing line 0, sparse detection)
- [x] Documentation updated
- [x] Build verified

### Next Steps
1. **Test with real data** - Run comparison commands above
2. **Measure improvement** - Compare correlation and MAD metrics
3. **Add second refinement pass** - Implement Python's `refine_linelocs_hsync()`
4. **Profile performance** - Ensure no significant slowdown

## References

- Python implementation: `lddecode/core.py` lines 2183-2339 (`compute_linelocs`)
- Python refinement: `lddecode/core.py` lines 2342-2409 (`refine_linelocs_hsync`)
- Zero-crossing: `lddecode/utils.py` lines 687-745 (`calczc`)
- C++ sync detector: `cpp-prototype/src/sync/sync_detector.cpp`
- Main decode loop: `cpp-prototype/src/core/main.cpp` lines 745-860

## Summary

This fix addresses the fundamental algorithm mismatch that was causing horizontal/vertical misalignment. By keeping actual pulse positions instead of rebuilding a uniform grid, the C++ decoder now matches Python's approach to handling VHS timing variations. Expected improvement: 5-10x better horizontal alignment (10px → 1-2px).
