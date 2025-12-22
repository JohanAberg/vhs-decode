# VHS RF Viewer - Enhanced Architecture Diagram

## System Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           VHS RF Viewer                                   │
│                     (Qt6 OpenGL Application)                              │
└──────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│                            Main Window                                    │
│  ┌────────────────────┐  ┌──────────────────────────────────────────┐   │
│  │   Control Panel    │  │         Video Display Area              │   │
│  │                    │  │  ┌────────────────────────────────────┐ │   │
│  │ ┌────────────────┐ │  │  │       VideoWidget (OpenGL)        │ │   │
│  │ │ Demod Params   │ │  │  │  ┌──────────────────────────────┐ │ │   │
│  │ │  - RF Bandpass │ │  │  │  │     Decoded Video Frame      │ │ │   │
│  │ │  - IRE Settings│ │  │  │  │   + Hair Cross Cursor        │ │ │   │
│  │ └────────────────┘ │  │  │  │   + Gamma/Exposure/Profile   │ │ │   │
│  │                    │  │  │  └──────────────────────────────┘ │ │   │
│  │ ┌────────────────┐ │  │  │                                    │ │   │
│  │ │ Color Mgmt     │ │  │  │  ┌──────────────┐                │ │   │
│  │ │  - Gamma       │ │  │  │  │ Histogram    │ (Overlay)      │ │   │
│  │ │  - Exposure    │ │  │  │  │ Widget       │                │ │   │
│  │ │  - Profile     │ │  │  │  └──────────────┘                │ │   │
│  │ └────────────────┘ │  │  │                                    │ │   │
│  │                    │  │  │  ┌──────────────┐                │ │   │
│  │ ┌────────────────┐ │  │  │  │ Vectorscope  │ (Overlay)      │ │   │
│  │ │ Analysis Tools │ │  │  │  │ Widget       │                │ │   │
│  │ │  - Histogram   │ │  │  │  └──────────────┘                │ │   │
│  │ │  - Vectorscope │ │  │  │                                    │ │   │
│  │ │  - Color Sample│ │  │  │  ┌──────────────┐                │ │   │
│  │ │  - Scanline    │ │  │  │  │ Scanline     │ (Overlay)      │ │   │
│  │ └────────────────┘ │  │  │  │ Plot Widget  │                │ │   │
│  │                    │  │  │  └──────────────┘                │ │   │
│  │ ┌────────────────┐ │  │  └────────────────────────────────────┘ │   │
│  │ │ Display        │ │  └──────────────────────────────────────────┘   │
│  │ │  - Brightness  │ │                                                  │
│  │ │  - Contrast    │ │  ┌──────────────────────────────────────────┐   │
│  │ └────────────────┘ │  │    ColorSamplerWidget                    │   │
│  └────────────────────┘  │  ┌────────┬─────────────────────────────┐│   │
│                          │  │Swatch  │ RGB: (128, 64, 255)        ││   │
│  ┌────────────────────┐  │  │[Color] │ Luma: 90                   ││   │
│  │ Transport Controls │  │  │        │ Position: (512, 288)       ││   │
│  │  - Play/Pause     │  │  └────────┴─────────────────────────────┘│   │
│  │  - Prev/Next Frame│  └──────────────────────────────────────────┘   │
│  │  - Loop           │                                                  │
│  └────────────────────┘  ┌──────────────────────────────────────────┐   │
│                          │         Timeline Widget                  │   │
│  ┌────────────────────┐  │  ┌────────────────────────────────────┐ │   │
│  │ Frame Label        │  │  │ [▓▓▓░░░░░░░░░▓▓▓░░|░░░░░░░▓▓]      │ │   │
│  │ Frame: 42/1000     │  │  │  In─┘        Cur─┘    Out─┘         │ │   │
│  └────────────────────┘  │  │  Cyan bars = Cached frames          │ │   │
│                          │  └────────────────────────────────────┘ │   │
│                          └──────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│                      Dockable Analysis Widgets                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │  Histogram      │  │  Vectorscope    │  │  Scanline Plot  │          │
│  │  (Docked Right) │  │  (Docked Right) │  │  (Docked Bottom)│          │
│  │  ┌───────────┐  │  │  ┌───────────┐  │  │  ┌───────────┐  │          │
│  │  │ ▅▄▆█▇▅▃   │  │  │  │  ╱•  •╲   │  │  │  │ ╱╲  ╱╲    │  │          │
│  │  │ ▃▂▅▆▇▄▂   │  │  │  │ ╱  •••  ╲ │  │  │  │╱  ╲╱  ╲   │  │          │
│  │  │ ▁▂▃▅▆▅▃   │  │  │  │•   •••   •│  │  │  │    ▁▂▃    │  │          │
│  │  │ 0 ═══ 255 │  │  │  │╲   •••  ╱ │  │  │  │0 ══════ W │  │          │
│  │  └───────────┘  │  │  └───────────┘  │  │  └───────────┘  │          │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
└──────────────────────────────────────────────────────────────────────────┘
```

## Data Flow Diagram

```
┌─────────────┐
│  .u8 File   │
│  (RF Data)  │
└──────┬──────┘
       │
       v
┌──────────────────────────────────────────────────────────────┐
│                    Decoder Worker Pool                        │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │ Thread 1   │  │ Thread 2   │  │ Thread 3   │  ...       │
│  │ RFProcessor│  │ RFProcessor│  │ RFProcessor│            │
│  │ FMDemod    │  │ FMDemod    │  │ FMDemod    │            │
│  │ SyncDetect │  │ SyncDetect │  │ SyncDetect │            │
│  │ TBCScaler  │  │ TBCScaler  │  │ TBCScaler  │            │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘            │
└────────┼───────────────┼───────────────┼────────────────────┘
         │               │               │
         v               v               v
   ┌─────────────────────────────────────────┐
   │         Frame Cache (LRU)               │
   │  ┌─────┬─────┬─────┬─────┬─────┐      │
   │  │ F41 │ F42 │ F43 │ F44 │ ... │      │
   │  └─────┴─────┴─────┴─────┴─────┘      │
   └─────────────────┬───────────────────────┘
                     │ Current Frame
                     v
   ┌───────────────────────────────────────────────────────┐
   │              VideoWidget (OpenGL)                     │
   │  ┌─────────────────────────────────────────────────┐  │
   │  │ 1. Color Transform (Gamma/Exposure/Profile)     │  │
   │  │    - Per-pixel adjustment                       │  │
   │  │    - Matrix-based color space conversion        │  │
   │  └─────────────────────────────────────────────────┘  │
   │  ┌─────────────────────────────────────────────────┐  │
   │  │ 2. Render Frame                                 │  │
   │  │    - OpenGL texture upload                      │  │
   │  │    - Zoom/Pan transformation                    │  │
   │  └─────────────────────────────────────────────────┘  │
   │  ┌─────────────────────────────────────────────────┐  │
   │  │ 3. Draw Hair Cross (if enabled)                 │  │
   │  │    - Green crosshair overlay                    │  │
   │  └─────────────────────────────────────────────────┘  │
   └───────────────────────────────────────────────────────┘
                     │
                     │ Frame Copy
                     v
   ┌─────────────────────────────────────────────────────────┐
   │           Analysis Widgets (Parallel)                   │
   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │
   │  │ Histogram   │  │ Vectorscope │  │ Scanline    │    │
   │  │             │  │             │  │ Plot        │    │
   │  │ 1. Compute  │  │ 1. RGB→YUV  │  │ 1. Extract  │    │
   │  │    bins     │  │    convert  │  │    scanline │    │
   │  │ 2. GPU      │  │ 2. GPU      │  │ 2. GPU      │    │
   │  │    render   │  │    render   │  │    render   │    │
   │  └─────────────┘  └─────────────┘  └─────────────┘    │
   └─────────────────────────────────────────────────────────┘
                     │
                     │ Mouse Position
                     v
   ┌─────────────────────────────────────────────────────────┐
   │          ColorSamplerWidget                             │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ 1. Read pixel at hair cross position              │  │
   │  │ 2. Extract RGB values                             │  │
   │  │ 3. Compute Luma                                   │  │
   │  │ 4. Display values and swatch                      │  │
   │  └───────────────────────────────────────────────────┘  │
   └─────────────────────────────────────────────────────────┘
```

## Class Hierarchy

```
QMainWindow
    └── MainWindow
        ├── Ui::MainWindow (generated)
        ├── DecoderWorker (owns)
        ├── FrameCache (owns)
        ├── HistogramWidget (owns)
        ├── VectorscopeWidget (owns)
        ├── ColorSamplerWidget (owns)
        ├── ScanlinePlotWidget (owns)
        └── Overlay widgets (raw pointers)

QOpenGLWidget
    ├── VideoWidget
    │   ├── OpenGL texture management
    │   ├── Color transform functions
    │   └── Hair cross rendering
    ├── HistogramWidget
    │   ├── Histogram computation (CPU)
    │   └── OpenGL bar rendering
    ├── VectorscopeWidget
    │   ├── YUV conversion (CPU)
    │   └── OpenGL point rendering
    └── ScanlinePlotWidget
        ├── Scanline extraction (CPU)
        └── OpenGL line rendering

QWidget
    ├── TimelineWidget
    │   └── Timeline painting/interaction
    └── ColorSamplerWidget
        └── Value display/color swatch

QObject
    ├── DecoderWorker
    │   └── Thread pool management
    └── FrameCache
        └── LRU cache logic
```

## Signal/Slot Connections

```
VideoWidget
    │
    ├─ hairCrossPositionChanged(pos) ────────┐
    │                                         │
    ├─ scanlineChanged(y) ──────────────┐    │
    │                                    │    │
    v                                    v    v
MainWindow                         Scanline  ColorSampler
    │                                 Plot    Widget
    ├─ onGammaChanged(value)         Widget
    ├─ onExposureChanged(value)
    ├─ onColorProfileChanged(index)
    ├─ onToggleHistogram()
    ├─ onToggleVectorscope()
    ├─ onToggleColorSampler()
    ├─ onToggleScanlinePlot()
    ├─ onHistogramOverlay()
    ├─ onVectorscopeOverlay()
    └─ onScanlinePlotOverlay()

DecoderWorker
    │
    └─ frameDecoded(frameNum, image) ─────> MainWindow
                                                │
                                                ├─> VideoWidget.setFrame()
                                                ├─> HistogramWidget.updateHistogram()
                                                ├─> VectorscopeWidget.updateVectorscope()
                                                └─> updateAnalysisWidgets()

TimelineWidget
    │
    ├─ frameSeek(frame) ───────────────────> MainWindow.onFrameSeek()
    ├─ inPointSet(frame) ──────────────────> MainWindow (status update)
    └─ outPointSet(frame) ─────────────────> MainWindow (status update)
```

## Widget Modes

```
┌─────────────────────────────────────────────────────────┐
│                  Analysis Widget Modes                   │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌─────────────────┐         ┌──────────────────┐      │
│  │  Dockable Mode  │         │  Overlay Mode    │      │
│  │                 │         │                  │      │
│  │  - QDockWidget  │         │  - Frameless     │      │
│  │  - Can dock     │         │  - Translucent   │      │
│  │    to edges     │         │  - Movable       │      │
│  │  - Resizable    │         │  - Resizable     │      │
│  │  - Float/Unfloat│         │  - Always on top │      │
│  │  - Title bar    │         │  - No title bar  │      │
│  └─────────────────┘         └──────────────────┘      │
│         │                            │                  │
│         v                            v                  │
│  ┌──────────────────────────────────────────────┐      │
│  │        Same Rendering Code                   │      │
│  │  - OpenGL/QPainter                           │      │
│  │  - GPU-accelerated                           │      │
│  │  - Real-time updates                         │      │
│  └──────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────┘
```

## Color Management Pipeline

```
Input Frame (RGB, Rec709-like)
    │
    ├─ Gamma != 2.2? ──> Apply Gamma Correction
    │                    (per-pixel power function)
    │
    ├─ Exposure != 0? ──> Apply Exposure
    │                     (linear multiplication)
    │
    └─ Profile != Native? ──> Color Space Transform
                              (3x3 matrix multiply)
                              │
                              ├─ Rec709 (default)
                              ├─ sRGB (gamma curve)
                              ├─ Rec2020 (wide gamut)
                              ├─ DCI-P3 (cinema)
                              └─ Adobe RGB (print)
    │
    v
Display Frame (transformed)
    │
    ├──> VideoWidget (OpenGL texture)
    ├──> HistogramWidget (compute bins)
    ├──> VectorscopeWidget (RGB→YUV)
    ├──> ColorSamplerWidget (pixel readback)
    └──> ScanlinePlotWidget (scanline extract)
```

## Memory Layout

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Memory                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Base Application:           ~50 MB                         │
│                                                              │
│  Decoder Components:         ~80 MB (4 threads × 20 MB)    │
│                                                              │
│  Frame Cache:                ~512 MB (configurable)         │
│    ├─ Single frame:         ~1-2 MB (1135×312 grayscale)  │
│    └─ Typical cache:        ~200-300 frames                 │
│                                                              │
│  Analysis Widgets:           ~5 MB                          │
│    ├─ Histogram:            ~1 KB (256×4 bins)             │
│    ├─ Vectorscope:          ~100-500 KB (point data)       │
│    ├─ ColorSampler:         ~1 KB (single pixel)           │
│    └─ ScanlinePlot:         ~10-50 KB (single line)        │
│                                                              │
│  OpenGL Resources:           ~10-20 MB                      │
│    ├─ Textures:             ~4 MB per frame                 │
│    ├─ Vertex buffers:       ~1-2 MB                        │
│    └─ Shader programs:      ~1 MB                          │
│                                                              │
│  ──────────────────────────────────────────────────────     │
│  Total (typical):            ~200-300 MB                    │
│  Total (with full cache):   ~650-700 MB                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## GPU Pipeline

```
┌────────────────────────────────────────────────────────────┐
│                      GPU Operations                         │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  VideoWidget:                                              │
│    Frame Data (CPU) ─┬─> Upload Texture (GPU)            │
│                      │   - Format: RGB32                   │
│                      │   - Size: 1135×312                  │
│                      │   - Filter: Linear                  │
│                      │                                      │
│                      └─> Transform Vertices (GPU)          │
│                          - Scale/Translate for zoom/pan    │
│                          - Draw textured quad              │
│                                                             │
│  HistogramWidget:                                          │
│    Histogram Data ────> Upload to VBO (GPU)               │
│    (CPU computed)      - Format: vec2 position + vec3 RGB  │
│                        - Draw instanced bars               │
│                                                             │
│  VectorscopeWidget:                                        │
│    UV Points ─────────> Upload to VBO (GPU)               │
│    (CPU computed)      - Format: vec2 UV coordinates       │
│                        - Draw as GL_POINTS                 │
│                        - Blending for intensity            │
│                                                             │
│  ScanlinePlotWidget:                                       │
│    Scanline Data ─────> Upload to VBO (GPU)               │
│    (CPU extracted)     - Format: vec2 position per channel │
│                        - Draw as GL_LINE_STRIP × 4         │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

This architecture provides a solid foundation for professional video analysis while maintaining good performance and code organization.
