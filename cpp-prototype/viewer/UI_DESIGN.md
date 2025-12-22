# VHS RF Viewer UI Design

## Main Window Layout

```
┌─────────────────────────────────────────────────────────────────┐
│ File   View                                          [_][□][X]   │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌───────────────────────────────────┬──────────────────────┐   │
│  │                                   │ Demodulation Params  │   │
│  │                                   │ ┌──────────────────┐ │   │
│  │        VIDEO DISPLAY              │ │ RF Low:   1.3 MHz│ │   │
│  │                                   │ │ RF High:  5.78   │ │   │
│  │      [Decoded Video Frame]        │ │ IRE 0:    4.1    │ │   │
│  │                                   │ │ Hz/IRE:   7000   │ │   │
│  │   - Mouse wheel to zoom           │ └──────────────────┘ │   │
│  │   - Click and drag to pan         │                      │   │
│  │                                   │ Display              │   │
│  │                                   │ ┌──────────────────┐ │   │
│  │                                   │ │☑ Auto Contrast   │ │   │
│  │                                   │ │ Brightness: ━━●━━│ │   │
│  │                                   │ │ Contrast:   ━━●━━│ │   │
│  │                                   │ └──────────────────┘ │   │
│  │                                   │                      │   │
│  └───────────────────────────────────┴──────────────────────┘   │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ TIMELINE                                                  │    │
│  │ ▼                                    ▼                    │    │
│  │ IN                                   OUT                  │    │
│  │ ├────────────────────────────────────┤                   │    │
│  │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░   │    │
│  │                      ▲                                    │    │
│  │                    Frame 125                              │    │
│  │                                                           │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                   │
│  [▶ Play] [⏸ Pause]  [◀]  [▶]   Frame: 125 / 250   ☑ Loop      │
│                                                                   │
├─────────────────────────────────────────────────────────────────┤
│ Status: Cache: 45 frames, 128 MB, Hit rate: 85.2%               │
└─────────────────────────────────────────────────────────────────┘
```

## UI Elements

### Video Display Area
- **Size**: 800x600 minimum
- **OpenGL rendering** for smooth display
- **Interactive controls**:
  - Left mouse drag: Pan
  - Mouse wheel: Zoom in/out
  - Double-click: Reset view

### Timeline Widget
- **Visual representation** of entire file
- **Current position marker** (yellow vertical line)
- **In point marker** (green triangle, press 'I' key)
- **Out point marker** (red triangle, press 'O' key)
- **Shaded region** between in/out points shows active playback area
- **Click/drag** to scrub through frames
- **Frame markers** every N frames for reference

### Control Panel (Right Side)
- **Demodulation Parameters**:
  - RF Bandpass Low (MHz): 0.1 - 10.0
  - RF Bandpass High (MHz): 1.0 - 20.0
  - IRE 0 (MHz): 3.0 - 6.0
  - Hz per IRE: 1000 - 20000
  - Changes invalidate cache and trigger re-decode

- **Display Controls**:
  - Auto Contrast checkbox
  - Brightness slider: -100 to +100
  - Contrast slider: 50% to 200%

### Transport Controls (Bottom)
- **Play button**: Start playback (25 fps for PAL)
- **Pause button**: Pause playback
- **Previous frame**: Go back one frame
- **Next frame**: Advance one frame
- **Frame counter**: Shows current / total frames
- **Loop checkbox**: Enable continuous playback

### Status Bar
- Shows cache statistics
- Displays current operation
- Shows decode progress

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Ctrl+O | Open file |
| Ctrl+Q | Quit |
| Space | Play/Pause |
| Left/Right | Previous/Next frame (when timeline focused) |
| I | Set in point at current frame |
| O | Set out point at current frame |
| Ctrl++ | Zoom in |
| Ctrl+- | Zoom out |
| Ctrl+0 | Reset zoom |

## Color Scheme

- **Background**: Dark gray (#1e1e1e)
- **Timeline background**: Medium gray (#3c3c3c)
- **Timeline border**: Light gray (#646464)
- **Current position**: Yellow (#ffff00)
- **In point**: Green (#00c800)
- **Out point**: Red (#c80000)
- **Shaded region**: Semi-transparent blue (#6496c850)
- **Status text**: Light gray (#dcdcdc)

## Data Flow

```
User Opens File
     ↓
Load RF File
     ↓
Estimate Total Frames
     ↓
Initialize Decoder Threads (4 workers)
     ↓
User Navigates to Frame N
     ↓
Check Frame Cache
     │
     ├─→ Found → Display Immediately
     │
     └─→ Not Found → Add Decode Job
             ↓
        Decode in Worker Thread:
          RF Read → Filter → Demod → Sync → Scale → QImage
             ↓
        Cache Frame
             ↓
        Signal Main Thread
             ↓
        Display Frame
```

## Performance Features

1. **Multi-threaded decoding**: 4 worker threads process frames concurrently
2. **Priority queue**: Current frame gets highest priority
3. **Prefetching**: Automatically pre-decodes next 5 frames
4. **LRU cache**: 512 MB default, stores decoded frames
5. **Cache hit rate tracking**: Monitors cache effectiveness
6. **Async loading**: Non-blocking UI during decode

## Future Enhancements

- Waveform monitor overlay
- Spectrum analyzer view
- Dropout detection visualization
- Audio (Hi-Fi) playback
- Export decoded frames
- Parameter presets
- Batch processing mode
