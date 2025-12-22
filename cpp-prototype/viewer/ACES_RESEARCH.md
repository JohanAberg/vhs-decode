# ACES Color Management Research

## Overview

ACES (Academy Color Encoding System) is a color management framework developed by the Academy of Motion Picture Arts and Sciences. It provides a standardized workflow for color management in film and television production.

## Key Components

### 1. ACES Color Spaces

- **ACES2065-1**: Scene-referred linear color space (AP0 primaries)
- **ACEScg**: Working color space for CGI (AP1 primaries)
- **ACEScct / ACEScc**: Log-encoded working spaces for color grading

### 2. Input Device Transforms (IDTs)

Convert camera-specific or input device color spaces to ACES2065-1.

### 3. Look Modification Transforms (LMTs)

Creative looks applied in ACES color space.

### 4. Output Device Transforms (ODTs)

Convert from ACES to display-referred output spaces (Rec709, P3, etc.)

### 5. Reference Rendering Transform (RRT)

Converts scene-referred ACES to output-referred OCES.

## Implementation Considerations for VHS-Decode Viewer

### Challenges

1. **Input Color Space**: VHS captures are already in a device-dependent color space (PAL/NTSC composite video)
2. **No Scene-Referred Data**: VHS is display-referred, not scene-referred like camera RAW
3. **Historical Content**: VHS predates ACES; content was mastered for CRT displays
4. **Complexity**: Full ACES requires CTL (Color Transformation Language) interpreter

### Potential Benefits

1. **Standardized Color Pipeline**: Consistent color handling across applications
2. **HDR Output**: ACES supports HDR display outputs
3. **Professional Workflows**: Integration with modern post-production tools
4. **Future-Proofing**: Archival format that's display-agnostic

### Implementation Options

#### Option 1: Full ACES Implementation

**Pros:**
- Professional-grade color management
- HDR support
- Industry standard

**Cons:**
- Requires CTL interpreter (OpenColorIO)
- Complex implementation (~5000+ lines of code)
- Large dependency (OCIO library)
- May not provide meaningful benefits for VHS content

**Dependencies:**
```cmake
find_package(OpenColorIO REQUIRED)
target_link_libraries(vhs-rf-viewer PRIVATE OpenColorIO::OpenColorIO)
```

#### Option 2: Simplified ACES-Inspired Approach

**Pros:**
- Simpler implementation
- No additional dependencies
- Provides some ACES benefits without full complexity

**Cons:**
- Not "true" ACES
- Limited to specific output transforms

**Implementation:**
- Convert input (Rec709/PAL) to ACES AP1 working space
- Apply simplified RRT+ODT for common displays
- Use matrix transforms (no CTL required)

#### Option 3: ACES as Export Option Only

**Pros:**
- Minimal complexity in viewer
- Provides ACES for downstream workflows
- Export to OpenEXR with ACES metadata

**Cons:**
- No real-time ACES viewing
- Still requires OCIO for proper implementation

## Recommended Approach

For the VHS-Decode viewer, **Option 2 (Simplified ACES-Inspired)** is recommended:

### Rationale

1. **VHS is Display-Referred**: Full scene-referred ACES workflow doesn't apply
2. **Simplicity**: Avoid complex dependencies for marginal benefit
3. **Flexibility**: Provide multiple output transforms without OCIO
4. **Performance**: Matrix-based transforms are GPU-friendly

### Implementation Plan

1. **Add ACES AP1 Working Space**
   - Convert existing Rec709/PAL to ACES AP1
   - Use 3x3 matrix transform

2. **Implement Simplified Output Transforms**
   - Rec709 ODT (simplified)
   - sRGB ODT
   - P3 D65 ODT
   - Rec2020 ODT (for HDR)

3. **Tone Mapping**
   - Simple S-curve tone mapping (emulates RRT)
   - Configurable for SDR/HDR output

4. **Color Profile Selection**
   - Keep existing profiles (Rec709, sRGB, etc.)
   - Add "ACES-like" option that uses simplified transforms

### Matrix Transforms

```cpp
// Rec709 -> ACES AP1 (simplified)
const float REC709_TO_AP1[9] = {
     0.6131f,  0.3395f,  0.0474f,
     0.0701f,  0.9164f,  0.0134f,
     0.0206f,  0.1096f,  0.8698f
};

// ACES AP1 -> Rec709 (simplified)
const float AP1_TO_REC709[9] = {
     1.7051f, -0.6242f, -0.0809f,
    -0.1302f,  1.1407f, -0.0105f,
    -0.0240f, -0.1289f,  1.1530f
};

// Similar matrices for P3, Rec2020, etc.
```

### Simplified Tone Curve

```cpp
float acesToneMap(float x) {
    // Simplified ACES RRT tone curve
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}
```

## Code Structure

```cpp
// In VideoWidget or new ColorManager class

enum class ColorSpace {
    Native,      // As decoded (Rec709-like for PAL)
    Rec709,      // BT.709
    sRGB,        // sRGB (similar to Rec709 but different gamma)
    Rec2020,     // BT.2020 (HDR)
    P3_D65,      // DCI-P3 with D65 white point
    AdobeRGB,    // Adobe RGB (1998)
    ACES_AP1     // ACES AP1 working space (simplified)
};

class ColorTransform {
public:
    static void transformToWorkingSpace(QImage &image, ColorSpace from, ColorSpace to);
    static void applyOutputTransform(QImage &image, ColorSpace output);
    static void applyToneMapping(QImage &image, bool aces_like);
    
private:
    static void matrixTransform(float &r, float &g, float &b, const float matrix[9]);
    static float acesToneMap(float x);
};
```

## Testing Strategy

1. **Test Images**: Use color bars, grayscale ramps, saturated colors
2. **Compare Output**: Against reference implementations (OCIO, DaVinci Resolve)
3. **Visual Validation**: Ensure colors look reasonable
4. **Performance**: Ensure real-time performance maintained

## Future Enhancements

1. **Full OCIO Integration** (if demand exists)
2. **Custom LUTs** (Load/apply 3D LUTs)
3. **HDR Display Support** (Rec2020 ST.2084/HLG)
4. **ACES Metadata Export** (in EXR files)

## References

- ACES Central: https://acescentral.com
- ACES Documentation: https://www.acescentral.com/aces-documentation
- OpenColorIO: https://opencolorio.org
- AMPAS GitHub: https://github.com/ampas/aces-dev

## Decision

For the initial implementation, we will:

1. **NOT implement full ACES** (too complex for marginal benefit)
2. **Add ACES-inspired simplified transforms** (matrix-based)
3. **Keep existing color profile system** (extensible)
4. **Document limitations** (not "true" ACES)
5. **Consider OCIO in future** (if needed)

This provides a good balance of functionality, simplicity, and performance for VHS content viewing.
