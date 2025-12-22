# VHS RF Viewer - UI Mockup (Text Version)

## Main Window Layout

```
╔════════════════════════════════════════════════════════════════════════════╗
║ VHS RF Viewer                                              ▢  □  ✕          ║
╠════════════════════════════════════════════════════════════════════════════╣
║ File   Edit   View   Help                                                  ║
╠════════╤═══════════════════════════════════════════════════════════════════╣
║        │                                                                    ║
║ DEMOD  │                    VIDEO DISPLAY                                  ║
║ PARAMS │   ┌────────────────────────────────────────────────────────────┐  ║
║        │   │                                                            │  ║
║ RF Low │   │  ┌──────────────┐                                          │  ║
║ 1.3 MHz│   │  │ Histogram    │  ◄─ Overlay (movable)                   │  ║
║        │   │  │ ▅▄▆█▇▅▃      │                                          │  ║
║ RF High│   │  │ ▃▂▅▆▇▄▂      │                                          │  ║
║ 5.78MHz│   │  │ ▁▂▃▅▆▅▃      │                                          │  ║
║        │   │  └──────────────┘                                          │  ║
║ IRE 0  │   │                                                            │  ║
║ 4.1 MHz│   │         ╔═══════════════════════════════╗                 │  ║
║        │   │         ║                               ║                 │  ║
║ Hz/IRE │   │         ║   [Video Frame Content]       ║                 │  ║
║ 7000   │   │         ║                               ║                 │  ║
║        │   │         ║       ┼ ← Hair cross          ║                 │  ║
║────────│   │         ║                               ║                 │  ║
║ COLOR  │   │         ║                               ║                 │  ║
║  MGMT  │   │         ║                               ║                 │  ║
║        │   │         ╚═══════════════════════════════╝                 │  ║
║ Gamma  │   │                                                            │  ║
║ ──●────│   │  ┌──────────────┐    ┌──────────────┐                    │  ║
║  2.2   │   │  │ Vectorscope  │    │ Scanline     │                    │  ║
║        │   │  │  ╱•  •╲       │    │ Plot         │                    │  ║
║Exposure│   │  │ ╱  •••  ╲     │    │ ╱╲  ╱╲       │                    │  ║
║ ──●────│   │  │•   •••   •    │    │╱  ╲╱  ╲      │                    │  ║
║  0.0   │   │  │╲   •••  ╱     │    │    ▁▂▃       │                    │  ║
║        │   │  └──────────────┘    └──────────────┘                    │  ║
║Profile │   │  ◄─ Overlays (movable)                                    │  ║
║[Rec709]│   └────────────────────────────────────────────────────────────┘  ║
║   │    │                                                                    ║
║   ▼    │   ┌────────────────────────────────────────────────────────────┐  ║
║────────│   │ COLOR SAMPLER                                              │  ║
║ANALYSIS│   │ ┌──┐  Pos: (512, 288)  RGB: (128, 64, 255)  Luma: 90     │  ║
║ TOOLS  │   │ │██│  ▓▓▓▓▓▓▓░░░░  ▓▓░░░░░░░░  ▓▓▓▓▓▓▓▓▓▓                │  ║
║        │   │ └──┘  Red ──────  Green ────  Blue ──────                 │  ║
║[✓]Hist │   └────────────────────────────────────────────────────────────┘  ║
║[✓]Vect │                                                                    ║
║[✓]Sampl│   ┌────────────────────────────────────────────────────────────┐  ║
║[✓]Scan │   │ TIMELINE                                                   │  ║
║        │   │ ┌────────────────────────────────────────────────────────┐ │  ║
║Overlays│   │ │[▓▓▓░░░░░░░░░▓▓▓░░|░░░░░░░░▓▓]                          │ │  ║
║[✓]Hist │   │ │ In─┘        Cur─┘    Out─┘                             │ │  ║
║[✓]Vect │   │ │ ▪▪▪▪  ▪▪   ▪▪▪ ▪ ▪     ▪▪  ← Cyan = Cached frames     │ │  ║
║[✓]Scan │   │ └────────────────────────────────────────────────────────┘ │  ║
║        │   └────────────────────────────────────────────────────────────┘  ║
║────────│                                                                    ║
║DISPLAY │   ┌─────┬──────┬──────┬──────┬──────┐  Frame: 42 / 1000         ║
║        │   │  ◄  │  ▶   │  ▶▶  │  ■   │ Loop │  Cache: 256 frames         ║
║Bright  │   └─────┴──────┴──────┴──────┴──────┘  25.0 fps                  ║
║ ──●────│                                                                    ║
║        │                                                                    ║
║Contrast│                                                                    ║
║ ──●────│                                                                    ║
║        │                                                                    ║
╚════════╧════════════════════════════════════════════════════════════════════╝
```

## Docked Analysis Widgets (Alternative Layout)

```
╔════════════════════════════════════════════════════════════════════════════╗
║ VHS RF Viewer                                              ▢  □  ✕          ║
╠════════════════════════════════════════════════════╤═══════════════════════╣
║                                                    │  Histogram            ║
║                 VIDEO DISPLAY                      │  ┌─────────────────┐  ║
║   ╔═══════════════════════════════════════════╗   │  │ ▅▄▆█▇▅▃         │  ║
║   ║                                           ║   │  │ ▃▂▅▆▇▄▂  RGB    │  ║
║   ║                                           ║   │  │ ▁▂▃▅▆▅▃  Luma   │  ║
║   ║         [Video Frame Content]             ║   │  │ 0 ═══════ 255   │  ║
║   ║                                           ║   │  └─────────────────┘  ║
║   ║                                           ║   ├───────────────────────┤
║   ║                                           ║   │  Vectorscope          ║
║   ╚═══════════════════════════════════════════╝   │  ┌─────────────────┐  ║
║                                                    │  │   ╱•  •╲        │  ║
║   ┌─────────────────────────────────────────┐     │  │  ╱  •••  ╲      │  ║
║   │ COLOR SAMPLER                           │     │  │ •   •••   •     │  ║
║   │ Pos:(512,288) RGB:(128,64,255) Y:90     │     │  │  ╲   •••  ╱     │  ║
║   └─────────────────────────────────────────┘     │  │   ╲_____╱       │  ║
║                                                    │  │    R Cy G Mg    │  ║
║   ┌─────────────────────────────────────────┐     │  └─────────────────┘  ║
║   │ TIMELINE   Frame: 42/1000               │     └───────────────────────┤
║   │ [▓▓▓░|░░░░▓▓]  ▪▪ ▪ ▪▪  ▪              │                              ║
║   └─────────────────────────────────────────┘                              ║
╠════════════════════════════════════════════════════════════════════════════╣
║  Scanline Plot                                                             ║
║  ┌──────────────────────────────────────────────────────────────────────┐  ║
║  │  ╱╲  ╱╲    ╱╲     ╱╲  ╱╲                  RGB + Luma                 │  ║
║  │ ╱  ╲╱  ╲  ╱  ╲   ╱  ╲╱  ╲   Line: 288    ▬▬ Red                      │  ║
║  │        ▁▂▃      ▁▂▃                       ▬▬ Green                    │  ║
║  │0 ════════════════════════════════════════ W   ▬▬ Blue                 │  ║
║  └──────────────────────────────────────────────────────────────────────┘  ║
╚════════════════════════════════════════════════════════════════════════════╝
```

## Widget Interaction Flow

```
USER INTERACTIONS                    WIDGET RESPONSES
═════════════════                    ═════════════════

1. Open RF File                      → Decoder starts
   ↓                                 → Frame cache fills
   Frame displayed                   → Analysis widgets update
   
2. Toggle Histogram                  → Histogram dock appears
   ↓                                 → Computes RGB/Luma bins
   Histogram shows                   → Real-time updates

3. Enable Color Sampler              → Hair cross appears on video
   ↓                                 → Mouse tracking enabled
   Move mouse over video             → Pixel values update
   ↓                                 → ColorSamplerWidget refreshes
   Scanline plot updates             → Shows horizontal line at Y

4. Click "Histogram Overlay"         → Creates floating overlay
   ↓                                 → Semi-transparent background
   Drag overlay                      → Moves with mouse
   ↓                                 → Stays on top of video
   Toggle off                        → Overlay disappears

5. Adjust Gamma Slider               → Per-pixel transform applied
   ↓                                 → Video brightens/darkens
   Value = 2.8                       → Analysis widgets update

6. Change Color Profile              → Color space transform
   ↓                                 → Matrix multiply on pixels
   Select "DCI-P3"                   → Video adjusts to cinema gamut
   ↓                                 → Analysis widgets recompute

7. Playback                          → 25 fps frame updates
   ↓                                 → All visible widgets update
   Cache hit rate: 95%               → Smooth playback
```

## Color Management Visualization

```
INPUT (Rec709-like from decoder)
       │
       ├──[Gamma Adjustment]─────────────┐
       │   • Value: 0.1 - 3.0             │
       │   • Default: 2.2                 │
       │   • Formula: out = in^(1/gamma)  │
       │                                  │
       ├──[Exposure Adjustment]───────────┤
       │   • Value: -5.0 to +5.0 stops    │
       │   • Default: 0.0                 │
       │   • Formula: out = in * 2^exp    │
       │                                  │
       └──[Color Profile Transform]───────┤
           • Rec709 (default)             │
           • sRGB                         │
           • Rec2020 (wide gamut)         │
           • DCI-P3 (cinema)              │
           • Adobe RGB                    │
           • Formula: RGB_out = M × RGB_in│
           ↓                              ↓
OUTPUT (Display-ready)          ANALYSIS WIDGETS
  • VideoWidget                   • Histogram (bins)
  • OpenGL texture                • Vectorscope (YUV)
  • Smooth zoom/pan               • ColorSampler (readback)
                                  • ScanlinePlot (extract)
```

## Analysis Widget Details

### Histogram
```
┌─────────────────────────────┐
│ Histogram                   │
├─────────────────────────────┤
│         ▆█▇                 │
│      ▄▅▆███▅▄               │
│   ▂▃▅███████▆▄▂             │
│ ▁▂▄▆████████▇▅▃▁            │
│▁▃▅▇█████████████▇▅▃▁        │
├─────────────────────────────┤
│ 0                       255 │
│                             │
│ [✓] RGB  [✓] Luma          │
│ [ ] Log Scale              │
└─────────────────────────────┘
```

### Vectorscope
```
┌─────────────────────────────┐
│ Vectorscope                 │
├─────────────────────────────┤
│         ┌───┐               │
│      ╱  │ G │  ╲            │
│     ╱   └───┘   ╲           │
│   Cy    •••••    R          │
│  ┌──┐  •••••••  ┌──┐        │
│  │Cy│ •••••••••• │R │        │
│  └──┘  •••••••••  └──┘       │
│        •••••••••             │
│   B     •••••    Mg         │
│     ╲   ┌───┐   ╱           │
│      ╲  │ B │  ╱            │
│         └───┘               │
└─────────────────────────────┘
```

### Color Sampler
```
┌─────────────────────────────────────────┐
│ Color Sampler                           │
├─────────────────────────────────────────┤
│ ┌───┐  Position: (512, 288)            │
│ │███│  RGB: (128, 64, 255)              │
│ │███│  Luma: 90                         │
│ └───┘                                   │
│      R: ████████░░░░░░░░ 128           │
│      G: ████░░░░░░░░░░░░ 64            │
│      B: ████████████████ 255           │
└─────────────────────────────────────────┘
```

### Scanline Plot
```
┌─────────────────────────────────────────┐
│ Scanline Plot - Line 288                │
├─────────────────────────────────────────┤
│ 255 ╱╲    ╱╲      ╱╲                   │
│     ╱  ╲  ╱  ╲    ╱  ╲                  │
│    ╱    ╲╱    ╲  ╱    ╲                 │
│   ╱            ╲╱      ╲                │
│  ╱                      ╲               │
│ 0 ──────────────────────────────────    │
│   0     256    512    768    1024       │
│                                          │
│   ▬▬ Red   ▬▬ Green   ▬▬ Blue   ▬▬ Y   │
└─────────────────────────────────────────┘
```

## Feature Highlights

### GPU Acceleration
- **Histogram**: CPU binning, GPU bar rendering
- **Vectorscope**: CPU YUV conversion, GPU point rendering
- **Scanline**: CPU extraction, GPU line rendering
- **Result**: 25 fps real-time analysis

### Dockable Interface
- Dock to any edge (left, right, top, bottom)
- Float as separate window
- Resize and rearrange
- Hide/show individual widgets

### Overlay Mode
- Semi-transparent background
- Drag to reposition
- Always on top of video
- Quick toggle on/off

### Real-time Updates
- Playback: All widgets update at 25 fps
- Scrubbing: Instant response
- Mouse: Hair cross tracks smoothly
- Sliders: Immediate visual feedback

This mockup demonstrates the comprehensive professional-grade analysis and color management capabilities now available in the VHS RF Viewer.
