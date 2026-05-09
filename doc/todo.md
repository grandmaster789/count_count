# CountVonCount — Known Weak Points / TODO

All identified issues have been resolved.

---

## Previously Fixed

- ~~Infinite loop in `data_location.cpp`~~ — added `parent == current_path` break
- ~~Tooth/gap polarity inversion~~ — flipped mask to `>= threshold`
- ~~Buffer overflow in `camera_manager.cpp`~~ — added size validation, RAII unlock, reject unknown formats
- ~~No error handling in main loop~~ — added try/catch, static image fallback
- ~~Inconsistent error reporting~~ — convention documented in CLAUDE.md
- ~~Excessive per-frame allocations~~ — buffers reused via memset/memcpy
- ~~`initialize_buffers()` wasted work~~ — buffers now properly reused
- ~~Min/max distance loop wrapping~~ — replaced with step-counted loop
- ~~`abs()` on doubles~~ — changed to `std::abs()` with `<cmath>`
- ~~`calculate_mean` division by zero~~ — added empty guard
- ~~`arc_length` infinite loop~~ — replaced while-loop with `std::fmod`
- ~~October "Okt" typo~~ — fixed to "Oct"
- ~~9x9 median blur O(81*W*H)~~ — replaced with integral image, O(1) per pixel
- ~~No bounds checking in `Image::at()`/`ptr()`~~ — added assert guards
- ~~`Image` constructor accepts negative dimensions~~ — added assert guards to constructors and `create()`
- ~~`ToothMeasurement` uninitialized members~~ — added default initializers for all members
- ~~`UnionFind` dead code~~ — removed from `boundary_trace.cpp`
- ~~Bounding-box marking merges separate contours~~ — replaced with flood-fill
- ~~Angle wrapping bug in visualization~~ — added `angle_midpoint()` helper using `std::fmod`
- ~~Settings deserialization no validation~~ — added delimiter checking and value clamping in `resolution.cpp` and `settings.cpp`
- ~~`SettingsManager::load()` returns true on parse failure~~ — now checks stream state and resets to defaults
- ~~`save_jpg` misleading error message~~ — removed `stbi_failure_reason()` from write path
- ~~`save_image` writes to CWD~~ — now saves to data directory
- ~~`SettingsManager` destructor can throw~~ — wrapped `save()` in try/catch
- ~~`foreground.cpp` assumes 3-channel input~~ — added channel count guard
- ~~Non-ASCII file paths~~ — changed to `generic_string()` for stb calls
- ~~`static_image.clone()` every frame~~ — clone only once using `static_image_loaded` flag
- ~~Camera converters allocate every frame~~ — changed `grab_frame` to take output reference, converters write into pre-allocated buffer
- ~~`RegisterClassExA`/`CreateWindowExA` unchecked~~ — added error checking with `GetLastError()` logging
- ~~`LOWORD`/`HIWORD` for mouse coords~~ — replaced with `GET_X_LPARAM`/`GET_Y_LPARAM`
- ~~`#include <formaT>` typo~~ — fixed to `<format>`
- ~~Redundant forward declarations~~ — removed from `application.h`
- ~~Missing `#include <cstring>`~~ — added to `camera_manager.cpp`
- ~~Orphaned HSV tests~~ — removed `bgr_to_hsv` helper and pure-conversion tests, kept only production `determine_foreground` integration test
- ~~`MFStartup` failure not tracked~~ — added `m_MFInitialized` flag, guard in `initialize()` and destructor
- ~~YUY2 reads past buffer for odd widths~~ — round width to even before processing
- ~~Centroid division by zero~~ — added empty contour guard returning origin
- ~~Distance threshold outlier-sensitive~~ — replaced min/max midpoint with P25/P75 percentile midpoint
- ~~Flood-fill duplicate pushes~~ — mark visited before pushing to bound stack to foreground pixel count
- ~~Double semicolon in `anomalies.cpp`~~ — removed
- ~~Redundant `SetWindowLongPtrA`~~ — removed from constructor, already handled in `WM_CREATE`
- ~~Inconsistent include guards~~ — standardized all 9 non-conforming headers to `CC_<MODULE>_<NAME>_H`
- ~~String formatting in visualization~~ — replaced concatenation with `std::format`
- ~~Test coverage gaps~~ — added tests for single-element mask, empty centroid, area threshold boundary
- ~~Camera frame stride not queried~~ — converters assumed stride=width; now queries `MF_MT_DEFAULT_STRIDE` and uses actual stride for NV12 UV-plane offset, row offsets in all converters, and RGB24 orientation
- ~~DIB row stride not 4-byte aligned~~ — `blit_image` now pads rows to 4-byte boundary when `cols*channels` is not a multiple of 4
- ~~`BLACKONWHITE` stretch mode corrupts colors when scaling~~ — switched to `HALFTONE` mode with `SetBrushOrgEx` for proper color averaging
- ~~No `WM_PAINT` handler~~ — added handler using paint DC so image redraws correctly when window is uncovered/resized
- ~~Gap anomaly shows "?" instead of tooth count~~ — now shows `found/expected` (e.g., "31/32") in red
- ~~Tooth count text too small~~ — font scale now proportional to image height (`rows/100`)
- ~~Text unreadable over gear surface~~ — added black background rectangle and scaled shadow offset
- ~~Arc anomaly arrows hidden when gap anomalies present~~ — both anomaly types now always visualized
