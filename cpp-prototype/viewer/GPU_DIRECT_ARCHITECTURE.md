# GPU-Direct Frame Format Architecture

## Current Approach (QImage)

The current implementation uses `QImage` as an intermediate format:

```
Decoder → uint16 TBC → uint8 grayscale → QImage → QOpenGLTexture → GPU
```

**Pros:**
- Simple API
- Qt handles format conversion
- Easy to work with in CPU code

**Cons:**
- Extra memory copy (TBC → QImage)
- Format conversion overhead
- CPU-side pixel processing

## GPU-Direct Alternatives

### Option 1: Direct Pixel Buffer Objects (PBO)

Use OpenGL Pixel Buffer Objects to upload directly to GPU without CPU copy:

```cpp
class GPUFrameBuffer {
    GLuint pbo_;
    GLuint texture_;
    
    void uploadFrame(const uint16_t *tbcData, size_t width, size_t height) {
        // Bind PBO for asynchronous upload
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_);
        
        // Map buffer and write TBC data directly
        void *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        memcpy(ptr, tbcData, width * height * sizeof(uint16_t));
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        
        // Upload to texture (asynchronous)
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
                       GL_RED, GL_UNSIGNED_SHORT, nullptr);
        
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
};
```

**Benefits:**
- Zero-copy upload to GPU
- Asynchronous DMA transfer
- Native TBC format (uint16)

### Option 2: Shared Texture Memory

Use OpenGL/OpenCL interop for zero-copy between decoder and renderer:

```cpp
class SharedGPUFrame {
    cl_mem clBuffer_;     // OpenCL buffer
    GLuint glTexture_;    // OpenGL texture
    
    void decodeToGPU(cl_command_queue queue, const uint8_t *rfData) {
        // Decode directly to OpenCL buffer (already on GPU from decoder)
        cl_enqueue_NDRange_kernel(queue, decoderKernel, ...);
        
        // Acquire for OpenGL use (zero-copy)
        clEnqueueAcquireGLObjects(queue, 1, &clBuffer_, ...);
        
        // Texture is now available to OpenGL without CPU roundtrip
    }
};
```

**Benefits:**
- True zero-copy (decoder writes directly to display buffer)
- Maximum performance
- Ideal for GPU-accelerated decoder

### Option 3: Vulkan Memory Import

Use Vulkan for more modern GPU pipeline:

```cpp
class VulkanFrameBuffer {
    VkDeviceMemory memory_;
    VkImage image_;
    
    void uploadFrame(const uint16_t *tbcData) {
        // Map Vulkan memory
        void *data;
        vkMapMemory(device, memory_, 0, size, 0, &data);
        memcpy(data, tbcData, size);
        vkUnmapMemory(device, memory_);
        
        // Image is ready for rendering (can be used in Qt Quick Scene Graph)
    }
};
```

**Benefits:**
- Modern API
- Better multi-threading
- Cross-platform (Windows, Linux, macOS, Android)

## Recommended Architecture

For this project, a hybrid approach is recommended:

### Phase 1: Current QImage (Already Implemented)
- Keep for simplicity and debugging
- Works everywhere Qt is supported
- Good baseline performance

### Phase 2: Optional PBO Path (Performance Enhancement)
```cpp
class VideoWidget : public QOpenGLWidget {
    enum class RenderPath {
        QImage,      // Current implementation
        PBO,         // Direct GPU upload
        Shared       // OpenCL/OpenGL interop
    };
    
    RenderPath renderPath_;
    
    void setFrameFromTBC(const uint16_t *tbcData, size_t width, size_t height) {
        if (renderPath_ == RenderPath::PBO) {
            uploadViaPBO(tbcData, width, height);
        } else {
            // Convert to QImage (current path)
            QImage image = convertTBCToQImage(tbcData, width, height);
            setFrame(image);
        }
    }
};
```

### Phase 3: Full GPU Pipeline (Future)
- Decoder produces OpenCL buffer
- Shared with OpenGL for display
- No CPU roundtrip

## Implementation Recommendation

**For current PR:**
1. Keep QImage approach ✓ (working, simple)
2. Document architecture for GPU-direct ✓ (this file)
3. Add interface for future enhancement ✓ (see below)

**Future enhancement interface:**
```cpp
// In videowidget.h
class VideoWidget : public QOpenGLWidget {
public:
    // Current method (keep)
    void setFrame(const QImage &frame);
    
    // Future GPU-direct methods (add interface)
    void setFrameFromTBC(const uint16_t *tbcData, size_t width, size_t height);
    void setFrameGPUBuffer(GLuint textureId, size_t width, size_t height);
    
    // Enable/disable GPU-direct path
    void setGPUDirectEnabled(bool enabled);
    bool isGPUDirectSupported() const;
    
private:
    bool gpuDirectEnabled_;
    std::unique_ptr<GPUFrameBuffer> gpuBuffer_;
};
```

## Memory Comparison

### Current (QImage):
```
RF Data (GPU) → Decoded TBC (CPU) → QImage (CPU) → Texture Upload → GPU Display
              └─ 2.4 MB              └─ 1.1 MB       └─ 1.1 MB
Total: ~4.6 MB per frame in flight
```

### GPU-Direct (PBO):
```
RF Data (GPU) → Decoded TBC (CPU) → PBO Upload → GPU Display
              └─ 2.4 MB            └─ 2.4 MB (async)
Total: ~2.4 MB per frame in flight (50% reduction)
```

### GPU-Direct (Shared):
```
RF Data (GPU) → Decoded TBC (GPU) → Shared Texture → GPU Display
              └─────── 2.4 MB (single allocation) ──────────┘
Total: ~2.4 MB per frame in flight (50% reduction, zero-copy)
```

## Performance Impact

Expected improvements with GPU-direct:

| Operation | QImage Path | PBO Path | Shared Path |
|-----------|-------------|----------|-------------|
| Frame upload | ~5-10 ms | ~1-2 ms | ~0.1 ms |
| Memory copies | 2 | 1 | 0 |
| CPU overhead | High | Medium | Low |
| GPU idle time | Yes | Minimal | None |

## Recommendation Summary

1. **Keep QImage for now** - It works, it's debuggable, it's cross-platform
2. **Document GPU-direct path** - This file provides the roadmap
3. **Add abstraction layer** - Interface allows switching render paths
4. **Implement PBO path later** - When performance becomes critical
5. **Shared path is future work** - Requires GPU decoder first

## Export Considerations

For export (EXR, ProRes), QImage is actually beneficial:

- **EXR**: Needs pixel data on CPU for OpenEXR library
- **ProRes**: FFmpeg needs CPU-side AVFrame
- **Both**: QImage provides easy format conversion

GPU-direct is primarily for **display performance**, not export.

For export workflows:
```
GPU Display Path: Decoder → GPU → Display (zero-copy)
Export Path:      Decoder → QImage → Exporter (CPU-side, OK)
```

## Cross-Platform Notes

| Platform | OpenGL | OpenCL | Vulkan | PBO | Shared |
|----------|--------|--------|--------|-----|--------|
| Linux    | ✓      | ✓      | ✓      | ✓   | ✓      |
| Windows  | ✓      | ✓      | ✓      | ✓   | ✓      |
| macOS    | ✓      | ✓      | ✓      | ✓   | Limited|

**macOS Note:** OpenCL deprecated, prefer Metal or Vulkan via MoltenVK

## Conclusion

The current QImage approach is **correct and sufficient** for the prototype. GPU-direct optimizations can be added later without changing the public API, thanks to the abstraction layer we're documenting here.

**Action items:**
- ✓ Document GPU-direct architecture (this file)
- ✓ Keep current QImage implementation
- ⚠ Add interface for future GPU-direct support (optional)
- ⚠ Implement PBO path (future enhancement)
