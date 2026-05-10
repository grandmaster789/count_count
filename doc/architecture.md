# CountVonCount Architecture

A Windows C++23 application that counts gear teeth in real-time using custom computer vision. No external image processing libraries — all algorithms, rendering, camera capture, and GUI are implemented from scratch using standard C++ and Win32 APIs.

---

## High-Level Overview

```
                          +---------------------+
                          |     Application      |
                          |   (main_loop())      |
                          +----------+----------+
                                     |
              +----------------------+----------------------+
              |                      |                      |
     +--------v--------+   +--------v--------+   +---------v--------+
     |  CameraManager  |   | SettingsManager  |   | MainWindow       |
     |  (MediaFound.)  |   | (load/save cfg)  |   | Controller       |
     +---------+-------+   +--------+--------+   | (Win32 GUI)      |
               |                     |            +---------+--------+
               v                     v                      |
         cc::Image              cc::Settings                v
          (BGR)                                      Win32 Window
               |                                    + Trackbar
               v                                    + Mouse input
     +---------+---------+
     | Processing Pipeline|
     +----+----+----+----+
          |    |    |    |
          v    v    v    v
      Foreground  Contour  Tooth   Anomaly
      Detection   Tracing  Count   Detection
          |    |    |    |
          v    v    v    v
     +----+----+----+----+
     |   Visualization    |
     |  (software render) |
     +--------------------+
```

---

## Directory Structure

```
src/
├── main.cpp                         # Entry point (Windows-only guard)
├── app/                             # Application orchestration
│   ├── application.h/.cpp           # Main loop, owns all managers
│   ├── camera_manager.h/.cpp        # MediaFoundation camera capture
│   ├── settings_manager.h/.cpp      # Config file persistence
│   └── main_window_controller.h/.cpp# Win32 native window + trackbar
├── processing/                      # Image processing pipeline
│   ├── foreground.h/.cpp            # Color/Chebyshev threshold + blur + invert (AVX2 SIMD)
│   ├── edge_mask.h/.cpp             # Edge-based foreground: BGR→luma→Sobel→flood-fill
│   ├── auto_sensitivity.h/.cpp      # Auto color+tolerance detection (histogram + Otsu)
│   ├── boundary_trace.h/.cpp        # Moore boundary tracing (contours)
│   ├── contours.h/.cpp              # Largest contour analysis + tooth pipeline
│   ├── centroid.h/.cpp              # Geometric center of contour
│   ├── count_teeth.h/.cpp           # Tooth enumeration, span filter, RANSAC pitch estimate
│   └── anomalies.h/.cpp             # Statistical 3-sigma anomaly detection
├── gui/                             # Rendering
│   ├── visualization.h/.cpp         # Result overlay (arrows, text, circles)
│   └── drawing.h/.cpp               # Primitives: lines, circles, text
├── io/                              # File I/O
│   ├── jpg.h/.cpp                   # STB-based JPEG load/save (RGB↔BGR)
│   └── data_location.h/.cpp         # Locate data/ folder from exe path
├── math/                            # Math utilities
│   ├── statistics.h/.inl            # Mean, variance (Bessel), stddev
│   ├── angles.h/.cpp                # Arc length with wrapping
│   └── square.h                     # Squaring helper template
├── types/                           # Core data types
│   ├── image.h                      # Image class (replaces cv::Mat)
│   ├── point.h                      # Point2<T> template (replaces cv::Point)
│   ├── color.h                      # Color3 struct (replaces cv::Scalar)
│   ├── color_range.h/.cpp           # Min/max BGR bounds from tolerance
│   ├── resolution.h/.cpp            # Width/height with serialization
│   ├── settings.h/.cpp              # App settings with serialization
│   ├── tooth_measurement.h/.cpp     # Per-tooth geometry data
│   └── tooth_anomaly.h/.cpp         # Anomaly flag enum + mask factory
├── platform/                        # Platform abstraction
│   ├── platform.h/.cpp              # OS detection, Win32 includes
│   └── build_date.h/.inl            # Compile-time build date from __DATE__
└── util/
    └── logger.h                     # Singleton logger with level filtering

tests/
├── test_types.cpp                   # Image, Point2, Color3
├── test_statistics.cpp              # Variance, mean, stddev
├── test_count_teeth.cpp             # Tooth detection + counting
├── test_find_anomalies.cpp          # Anomaly detection on synthetic gears
├── test_jpg_io.cpp                  # JPEG round-trip via STB
├── test_boundary_trace.cpp          # Contour detection on synthetic masks
├── test_drawing.cpp                 # Line, circle, arrow, text rendering
├── test_image_ops.cpp               # Pixel ops, shoelace area, centroid
├── test_morphology.cpp              # Dilate, erode, morphological close
├── test_color_conversion.cpp        # BGR↔HSV, foreground mask stability
├── test_threshold.cpp               # Percentile vs midpoint threshold
└── test_smooth.cpp                  # Circular moving average
```

---

## Namespace Hierarchy

All code lives under `cc::` with sub-namespaces mirroring the folder layout:

```
cc::
├── Image, Color3, Point2<T>         # Core value types
├── ColorRange, Resolution, Settings # Configuration types
├── ToothMeasurement, ToothAnomaly   # Domain types
├── ePlatform                        # Platform enum
├── find_data_folder()               # I/O utility
│
├── app::
│   ├── Application                  # Main orchestrator
│   ├── CameraManager                # MediaFoundation capture
│   ├── SettingsManager              # Config persistence
│   └── MainWindowController         # Win32 window
│
├── processing::
│   ├── chebyshev_threshold()        # BGR Chebyshev distance → 0/255 mask (AVX2)
│   ├── invert_mask()                # Bitwise NOT on mask buffer (AVX2)
│   ├── majority_vote_blur()         # 9×9 box-sum majority vote via integral image (AVX2 lookup)
│   ├── determine_foreground()       # Full color/Chebyshev pipeline (calls above three)
│   ├── determine_foreground_by_edges() # Edge-based alternative pipeline
│   ├── detect_sensitivity()         # Auto-detect foreground color + tolerance
│   ├── detect_background_sensitivity() # Auto-detect background color + tolerance (for invert mode)
│   ├── find_contours()              # Boundary tracing
│   ├── process_contours()           # Full tooth pipeline
│   ├── find_centroid()              # Contour center
│   ├── find_tooth_start()           # First 0→1 transition
│   ├── count_teeth()                # Tooth enumeration with span filter
│   ├── estimate_tooth_count()       # RANSAC-style pitch estimation with sub-harmonic collapse
│   └── find_anomalies()             # 3σ statistical detection
│
├── drawing::
│   ├── draw_line()                  # Bresenham's algorithm
│   ├── draw_circle()                # Midpoint circle algorithm
│   ├── draw_arrowed_line()          # Line + arrowhead
│   ├── draw_polyline()              # Connected line segments
│   ├── draw_text()                  # Bitmap font rendering
│   └── measure_text()               # Text size calculation
│
├── io::
│   ├── load_jpg() / save_jpg()      # STB-based JPEG I/O
│   └── ImageError                   # Exception type
│
├── math::
│   ├── calculate_mean()             # Arithmetic mean
│   ├── calculate_variance()         # Bessel-corrected (N-1)
│   ├── calculate_standard_deviation()
│   └── arc_length()                 # Angular distance with wrapping
│
└── util::
    └── Logger                       # Singleton, level-filtered logging
```

---

## Processing Pipeline

Each frame follows this sequence inside `Application::main_loop()`:

### 1. Frame Acquisition

```
Camera (live)                     Static image
  │                                   │
  │  CameraManager::grab_frame()      │  io::load_jpg()
  │  MediaFoundation IMFSourceReader   │  STB stbi_load
  │  NV12/YUY2/RGB24 → BGR            │  RGB → BGR swap
  │                                   │
  └──────────────┬────────────────────┘
                 │
            cc::Image (BGR, 3-channel)
```

**CameraManager** uses MediaFoundation COM APIs:
- `MFEnumDeviceSources` for device enumeration
- `IMFSourceReader::ReadSample` for frame capture
- Custom YUV→BGR pixel conversion (NV12, YUY2, RGB24 formats)
- Resolution negotiation: tries 4K → 1080p → 720p → 480p

### 2. Foreground Detection

Three modes, cycled with **E**:

**Color threshold** (default) — `determine_foreground(..., use_chebyshev=true/false)`:
```
Source Image (BGR)  +  Selected Color  +  Tolerance
         │
         ├── Chebyshev mode (use_chebyshev=true):
         │     chebyshev_threshold() — AVX2, 32 pixels/iter
         │     dist = max(|B-ref_b|, |G-ref_g|, |R-ref_r|)
         │     mask = dist <= tolerance ? 255 : 0
         │
         └── HSV mode (use_chebyshev=false):
               Per-pixel BGR→HSV, hue/sat/val range test
         │
         majority_vote_blur() — 9×9 box-sum via integral image, AVX2 lookup
         │
         invert_mask() — AVX2 bitwise NOT (background subtraction mode only)
         │
         Copy with mask (src → dst where mask≠0)
```

**Edge detection** — `determine_foreground_by_edges()`:
```
BGR → luma (BT.601) → Sobel magnitude → mean+k·σ threshold
→ morphological close (2× dilate + 2× erode)
→ border flood-fill (exterior = 0, interior = 255)
```

**Background subtraction** — same as color threshold with `invert=true` and `use_chebyshev=true`. Auto-sensitivity in this mode calls `detect_background_sensitivity()` instead of `detect_sensitivity()`.

### 3. Contour Detection

```
Binary mask (1-channel)
         │
    find_contours()
    ├── Scan for unvisited boundary pixels (left-to-right, top-to-bottom)
    ├── Moore boundary tracing (8-connectivity, clockwise)
    ├── Mark visited pixels to avoid re-tracing
    └── Flood-fill interior as visited
         │
    vector<vector<Point2i>>  (all external contours)
```

**Moore boundary tracing** follows the boundary of each connected component clockwise using the 8-neighbor Moore neighborhood. The algorithm:
1. Find a boundary pixel (foreground pixel with at least one background neighbor)
2. Walk the boundary clockwise, recording each pixel
3. Stop when returning to the starting pixel

### 4. Contour Analysis

```
All contours
    │
    ├── Compute area of each (shoelace formula)
    ├── Select largest
    ├── Reject if area < 1% of image area
    │
    └── Largest contour
            │
    ┌───────┴───────┐
    │               │
find_centroid()   Draw contour (red)
    │
    ├── Point2d (double precision)
    ├── Point2f (float, for atan2)
    └── Point2i (integer, for pixel ops)
```

**Shoelace formula** computes signed polygon area:
```
A = ½ |Σᵢ (xᵢ·yᵢ₊₁ − xᵢ₊₁·yᵢ)|
```

**Centroid** is the average of all contour vertex coordinates.

### 5. Tooth Counting

```
Contour + Centroid
    │
    ├── Measure distance from each contour point to centroid
    │   distance[i] = hypot(pt.x - cx, pt.y - cy)
    │
    ├── Threshold = otsu_threshold(distances) — maximises between-class variance
    │
    ├── Create tooth mask: 1 if distance < threshold (gap), 0 if ≥ (tooth tip)
    │   Note: teeth protrude outward, so higher distance = tooth
    │   The mask uses 1 for "close to center" (gap) and 0 for "far" (tooth)
    │   But count_teeth looks at 0→1 transitions as tooth starts
    │
    ├── find_tooth_start(): first 0→1 transition in circular mask
    │
    └── count_teeth(): traverse mask from start position
        ├── Rising edge (0→1): begin new tooth measurement
        │   - Record contour index
        │   - Calculate starting angle via atan2
        └── Falling edge (1→0): complete measurement
            - Record ending angle
            - Wrap both angles to [0, 2π]
            - Find min/max distances in tooth region
            │
        vector<ToothMeasurement>
```

### 6. Anomaly Detection

```
vector<ToothMeasurement>
    │
    ├── Compute tooth arc lengths:  arc_length(start_angle, end_angle)
    ├── Compute gap arc lengths:    arc_length(end_angle, next_start_angle)
    │
    ├── Statistics (Bessel-corrected, divides by N-1):
    │   ├── mean_arc, stddev_arc
    │   └── mean_gap, stddev_gap
    │
    ├── For each tooth:
    │   ├── |arc - mean_arc| > 3σ_arc  →  flag as ToothAnomaly::arc
    │   └── |gap - mean_gap| > 3σ_gap  →  flag as ToothAnomaly::gap
    │
    vector<uint8_t>  (bit flags per tooth)
```

The 3-sigma threshold means only strong anomalies are flagged — roughly 0.3% false positive rate for normally distributed measurements.

### 7. Visualization

```
Output image (clone of source)
    │
    ├── Draw centroid (filled white circle, radius 8)
    │
    ├── For each anomaly:
    │   ├── Arc anomaly: radial arrow from center toward tooth
    │   └── Gap anomaly: V-shaped lines at tooth boundaries
    │
    └── Draw tooth count text at centroid:
        ├── Normal: green "N/N"
        ├── Arc anomalies: red "N/(N+anomalies)"
        └── Gap anomalies: red "?"
        (with black shadow offset for readability)
```

---

## Type System

### cc::Image

Contiguous row-major pixel buffer. Replaces `cv::Mat`.

```
┌─────────────────────────────────────────┐
│  Memory layout: [B G R] [B G R] ...    │
│  Row 0: pixels (0,0) (1,0) ... (W-1,0) │
│  Row 1: pixels (0,1) (1,1) ... (W-1,1) │
│  ...                                    │
│  Row H-1: ...                           │
└─────────────────────────────────────────┘

stride = cols * channels (bytes per row)
at(y, x) = data + y * stride + x * channels
```

| Method | Description |
|--------|-------------|
| `Image()` | Empty image |
| `Image(rows, cols, channels)` | Zero-initialized |
| `Image(rows, cols, channels, data)` | Copy from raw buffer |
| `empty()` | True if no data |
| `rows()`, `cols()`, `channels()` | Dimensions |
| `data()` | Raw pointer to first byte |
| `ptr(row)` | Pointer to start of row |
| `at(y, x)` | Pointer to first channel of pixel (y, x) |
| `clone()` | Deep copy |
| `create(rows, cols, channels)` | Reallocate and zero |
| `Image::zeros(rows, cols, channels)` | Static factory |

### cc::Point2\<T\>

2D point template with arithmetic operators.

| Alias | Type | Usage |
|-------|------|-------|
| `Point2i` | `Point2<int>` | Pixel coordinates, contour points |
| `Point2f` | `Point2<float>` | Angle calculations (atan2f) |
| `Point2d` | `Point2<double>` | High-precision centroid |

### cc::Color3

BGR color with double-precision channels. Supports `operator[]` for indexed access (0=B, 1=G, 2=R).

---

## GUI System

### Win32 Window

```
+------------------------------------------+
|  CountCount                        [─][□][×] |
+------------------------------------------+
|  [===|============] Sensitivity          |  ← TRACKBAR_CLASS child window
+------------------------------------------+
|                                          |
|            Image viewport                |  ← StretchDIBits blitting
|         (scales to window size)          |
|                                          |
|                                          |
+------------------------------------------+
```

- **Window class**: `CountCountWindowClass` registered once
- **Blitting**: `StretchDIBits` with top-down DIB (negative height in BITMAPINFOHEADER)
- **Input**: `WM_LBUTTONDOWN` for color picking (maps window coords to image coords), `WM_HSCROLL` for trackbar
- **Key handling**: `PeekMessage` loop with timeout (replaces `cv::waitKey`)
- **Lifecycle**: `WM_CLOSE` sets `m_IsOpen = false`

### Software Drawing

All drawing operates directly on `cc::Image` pixel buffers:

| Primitive | Algorithm | Complexity |
|-----------|-----------|------------|
| Line | Bresenham's line algorithm | O(max(dx, dy)) |
| Thick line | Bresenham + filled square per pixel | O(max(dx, dy) * thickness²) |
| Circle | Midpoint circle (8-way symmetry) | O(r) |
| Filled circle | Brute-force r² test | O(r²) |
| Arrow | Line + 2 angled lines at tip | O(length) |
| Text | 5×7 embedded bitmap font, integer scaling | O(chars * 35 * scale²) |

Font glyphs are stored as 7 bytes per character (5-bit rows), supporting: `0-9 / ? !` and space.

---

## Camera Capture

### MediaFoundation Pipeline

```
MFEnumDeviceSources()
    │
    └── IMFActivate[] (device list)
            │
    ActivateObject(IID_IMFMediaSource)
            │
    MFCreateSourceReaderFromMediaSource()
            │
    IMFSourceReader
    ├── GetNativeMediaType() → iterate to find resolution
    ├── SetCurrentMediaType() → lock format
    └── ReadSample()
            │
        IMFSample → IMFMediaBuffer → Lock()
            │
        Raw pixel data (NV12 / YUY2 / RGB24)
            │
        Custom conversion → cc::Image (BGR)
```

### Pixel Format Conversion

| Format | Layout | Conversion |
|--------|--------|------------|
| NV12 | Y plane + interleaved UV plane | Per-pixel YUV→RGB with ITU-R BT.601 coefficients |
| YUY2 | Packed YUYV (4 bytes = 2 pixels) | Per-pair YUV→RGB |
| RGB24 | Bottom-up BGR rows | Row flip (bottom-up → top-down) |

---

## Configuration

### Settings File Format (`data/count_count.cfg`)

```
<camera_id>
[<width> x <height>]
<B> <G> <R>
<tolerance>
```

Example:
```
0
[1920 x 1080]
45 120 80
50
```

Loaded on startup, saved on shutdown (via `SettingsManager` destructor).

---

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| **Stb** | 2024-07-29+ | `stb_image.h` for JPEG loading, `stb_image_write.h` for saving |
| **Catch2** | 3.4+ | Unit testing framework |
| **xsimd** | 14.1.0+ | Header-only SIMD abstraction; AVX2 acceleration for Chebyshev threshold, majority-vote blur lookup, and mask invert in `foreground.cpp` |

### Win32 System Libraries

| Library | Purpose |
|---------|---------|
| `mfplat.lib` | MediaFoundation platform APIs |
| `mf.lib` | MediaFoundation core |
| `mfreadwrite.lib` | `IMFSourceReader` for camera capture |
| `mfuuid.lib` | MediaFoundation GUIDs |
| `comctl32.lib` | Common controls (trackbar/slider) |

---

## Build System

CMake 3.10+ with vcpkg integration. Sources are auto-collected via `GLOB_RECURSE` — adding new `.cpp`/`.h` files requires no CMakeLists.txt changes.

### Targets

| Target | Type | Description |
|--------|------|-------------|
| `CountVonCount` | Executable | Links `main.cpp` + `CountVonCountLib` |
| `CountVonCountLib` | Static library | All `src/` except `main.cpp` |
| `CountVonCountTests` | Executable | All `tests/` + `CountVonCountLib` + Catch2 |

### Compiler Settings

| Compiler | Flags |
|----------|-------|
| MSVC (`CountVonCountLib`) | `/W4 /WX /MP /arch:AVX2`, defines `_CRT_SECURE_NO_WARNINGS` |
| MSVC (`CountVonCount`, `CountVonCountTests`) | `/W4 /WX /MP` (no `/arch:AVX2` — tests call SIMD code via the library ABI, not by including xsimd headers) |
| GCC/Clang | `-Wall -Wextra -Werror -Wpedantic` |

---

## Test Suite

18 test files covering all layers. Run with:

```bash
ctest --test-dir build -C Release --output-on-failure
# or run the binary directly:
build/release/CountVonCountTests.exe          # all tests
build/release/CountVonCountTests.exe [perf]   # perf tests only
build/release/CountVonCountTests.exe ~[perf]  # exclude perf tests
```

| File | What it covers |
|------|----------------|
| `test_types.cpp` | Image construction, clone, at(), Point2 arithmetic, Color3 indexing |
| `test_statistics.cpp` | Bessel-corrected variance, mean, stddev, percentile_threshold, otsu_threshold |
| `test_count_teeth.cpp` | Tooth start detection, counting, span filter, sub-harmonic collapse |
| `test_find_anomalies.cpp` | Empty input, single tooth, uniform gear, gap/arc anomalies, 3σ threshold |
| `test_jpg_io.cpp` | Load/save round-trip, error handling, special paths, large images |
| `test_boundary_trace.cpp` | Empty/single pixel, rectangles, L-shape, full white, gear-like mask |
| `test_drawing.cpp` | Lines, circles, arrows, polylines, text rendering, edge clipping |
| `test_image_ops.cpp` | in_range, copy_with_mask, shoelace area, centroid |
| `test_morphology.cpp` | Dilate, erode, morphological close, gap filling |
| `test_color_conversion.cpp` | HSV foreground (hue, saturation, wraparound), Chebyshev foreground (incl. wide-image SIMD path), invert |
| `test_threshold.cpp` | percentile_threshold and otsu_threshold: bimodal, outliers, uniform, edge cases |
| `test_pixel_verify.cpp` | Pixel-level verification of image operations |
| `test_smooth.cpp` | Constant signal, step, impulse, circular wrap, tooth signal preservation |
| `test_auto_sensitivity.cpp` | Color histogram, Otsu on distance, detect_sensitivity, detect_background_sensitivity |
| `test_edge_mask.cpp` | Edge-based foreground detection pipeline |
| `test_contours.cpp` | Contour processing pipeline integration |
| `test_visualization.cpp` | display_results label formatting (direct vs speculative count) |
| `test_perf.cpp` | Millisecond timing: chebyshev_threshold, invert_mask, majority_vote_blur, full pipelines (tagged `[perf]`, always pass) |
