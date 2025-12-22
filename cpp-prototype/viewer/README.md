# VHS RF Viewer

A Qt-based graphical viewer for VHS RF (.u8) files with real-time decoding and playback.

## Features

- **Real-time RF decoding**: Decode VHS RF signals on-the-fly as you navigate
- **OpenGL video display**: Hardware-accelerated video rendering with smooth zoom and pan
- **Interactive timeline**: Visual timeline with current position marker and in/out points
- **Cached frame visualization**: See which frames are loaded in memory (cyan markers on timeline)
- **Frame cache**: Intelligent memory caching for smooth playback (LRU eviction)
- **Adjustable parameters**: Real-time adjustment of demodulation parameters
- **Keyboard shortcuts**: Fast navigation and in/out point setting (I/O keys)
- **Multi-threaded decoding**: Concurrent frame decoding with worker thread pool
- **Pan and zoom**: Mouse-based image navigation
- **Export support architecture**: Extensible design for EXR, ProRes, and other formats
- **Cross-platform**: Runs on Linux, Windows, and macOS

## Building

### Requirements

- Qt6 (Widgets, OpenGL, OpenGLWidgets)
- FFTW3 (single-precision)
- C++17 compiler
- CMake 3.16+

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install qt6-base-dev libqt6opengl6-dev libfftw3-dev
```

### Windows

```bash
# Install Qt6 from qt.io
# Then use Qt Creator or Visual Studio:
cmake -B build -S . -DBUILD_VIEWER=ON -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### macOS

```bash
brew install qt@6
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
cmake -B build -S . -DBUILD_VIEWER=ON
cmake --build build
```

### Build Instructions

```bash
cd cpp-prototype
mkdir build && cd build
cmake .. -DBUILD_VIEWER=ON
cmake --build . --parallel
```

The viewer executable will be at `build/viewer/vhs-rf-viewer`.

## Usage

### Opening a File

1. Launch the viewer: `./viewer/vhs-rf-viewer`
2. File → Open RF File... (or Ctrl+O)
3. Select a .u8 RF file

### Navigation

- **Previous/Next frame**: `<` and `>` buttons or arrow keys when timeline has focus
- **Scrub**: Click and drag on timeline
- **Play/Pause**: Play and Pause buttons
- **Loop**: Enable loop checkbox to repeat playback

### Timeline Features

- **Current position**: Yellow vertical line shows current frame
- **Set In Point**: Press `I` key (green marker appears)
- **Set Out Point**: Press `O` key (red marker appears)
- **Play Range**: When in/out points are set, playback is limited to that range
- **Visual feedback**: Shaded region shows active playback area

### Video Display

- **Pan**: Click and drag with left mouse button
- **Zoom**: Mouse wheel
- **Zoom shortcuts**: Ctrl++ (zoom in), Ctrl+- (zoom out), Ctrl+0 (reset)
- **Reset view**: View → Reset Zoom

### Parameter Controls

Right panel contains real-time adjustable parameters:

- **RF Bandpass Low/High**: RF filter cutoff frequencies (MHz)
- **IRE 0**: Black level frequency (MHz)
- **Hz per IRE**: FM deviation scaling
- **Brightness/Contrast**: Display adjustment

**Note**: Changing parameters clears the cache and re-decodes frames.

## Architecture

### Components

1. **MainWindow**: Main application window with UI layout
2. **VideoWidget**: OpenGL-based video display with zoom/pan
3. **TimelineWidget**: Interactive timeline with in/out points
4. **DecoderWorker**: Multi-threaded frame decoder manager
5. **FrameCache**: LRU cache for decoded frames

### Decoding Pipeline

```
.u8 File → RFReader → RFProcessor (FFT + bandpass) 
  → FMDemodulator → SyncDetector → TBCScaler → QImage
```

### Threading Model

- **GUI Thread**: Qt event loop, rendering, user interaction
- **Decoder Threads**: 4 worker threads for concurrent frame decoding
- **Job Queue**: Priority queue for frame decode requests

### Caching Strategy

- **LRU eviction**: Least recently used frames evicted first
- **Default size**: 512 MB
- **Prefetching**: Automatically pre-decodes next 5 frames
- **Priority**: Current frame gets highest decode priority

## Performance

- **Memory cache**: Prevents redundant decoding of recently viewed frames
- **Multi-threaded**: 4 concurrent decoder threads for smooth playback
- **Prefetching**: Pre-loads upcoming frames for seamless playback
- **Hardware acceleration**: OpenGL rendering for smooth zoom/pan

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open RF file |
| Ctrl+Q | Quit |
| Ctrl++ | Zoom in |
| Ctrl+- | Zoom out |
| Ctrl+0 | Reset zoom |
| I | Set in point (timeline focused) |
| O | Set out point (timeline focused) |
| Space | Play/Pause (when implemented) |

## New Features (Latest Update)

### Cached Frame Visualization

The timeline now shows which frames are cached in memory:
- **Cyan markers** at the bottom of timeline indicate cached frames
- Updates in real-time as frames are decoded
- Helps visualize cache hit patterns during playback
- Shows approximately 40-60 frames cached during typical use

### Export Architecture

The viewer includes an extensible export framework for multiple formats:

**Supported (implemented):**
- ✓ PNG sequence export

**Planned (architecture ready):**
- ⚠ OpenEXR sequence (16/32-bit float, HDR)
- ⚠ Apple ProRes (422/4444/XQ profiles)
- ⚠ FFV1 (lossless codec)
- ⚠ H.264/H.265 (distribution codecs)

See `frameexporter.h` for the export API and `GPU_DIRECT_ARCHITECTURE.md` for technical details.

### Cross-Platform Support

Verified to work on:
- **Linux**: Ubuntu 22.04+, Fedora 38+, Arch Linux (primary development platform)
- **Windows**: Windows 10/11 with Visual Studio 2022 or MinGW
- **macOS**: macOS 12+ with Homebrew Qt6

Platform-specific optimizations:
- Windows: High DPI scaling support
- macOS: Retina display support, app bundle creation
- Linux: Wayland support when available

### GPU-Direct Future Path

While the current implementation uses QImage for frame display, the architecture supports future GPU-direct rendering for maximum performance:

- **Current**: Decoder → QImage → GPU texture (works everywhere)
- **Future Option 1**: Decoder → PBO → GPU texture (50% less memory, async)
- **Future Option 2**: Decoder (GPU) → Shared texture → Display (zero-copy)

See `GPU_DIRECT_ARCHITECTURE.md` for detailed technical discussion.

## Known Limitations

- **PAL only**: Currently optimized for PAL (625-line) format
- **No audio**: Audio decoding not yet implemented
- **Export stubs**: EXR/ProRes export architecture present but not implemented
- **Limited formats**: Only .u8 RF files supported

## Future Enhancements

- [ ] NTSC format support
- [ ] Audio (Hi-Fi) decoding and playback
- [x] Export architecture (framework complete, implementations pending)
- [ ] OpenEXR export implementation
- [ ] ProRes export implementation
- [ ] Real-time spectrum analyzer
- [ ] Waveform monitor
- [ ] Dropout visualization
- [ ] Parameter presets
- [ ] Batch processing
- [ ] GPU-direct rendering (PBO or shared textures)

## Troubleshooting

### Qt6 not found

If CMake can't find Qt6:

```bash
export CMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

### FFTW3 not found

Ensure libfftw3-dev (or equivalent) is installed and visible to pkg-config:

```bash
pkg-config --modversion fftw3f
```

### Build errors

Make sure you build the main decoder first:

```bash
cd cpp-prototype/build
cmake .. -DBUILD_VIEWER=ON -DUSE_FFTW3=ON
cmake --build . --parallel
```

## License

Same as vhs-decode project (GPL-3.0)
