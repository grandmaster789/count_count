# ADR-001: SIMD Acceleration via xsimd

**Date:** 2026-05-10  
**Status:** Accepted

---

## Context

CountVonCount processes 1920×1080 frames at interactive rates (~30 fps). Profiling the pipeline on the live-video path reveals three computationally heavy per-frame steps, each iterating over every pixel:

| Step | Location | Work per pixel |
|---|---|---|
| Chebyshev foreground threshold | `processing/foreground.cpp` | 3 abs-diffs, 2 max, 1 compare |
| BGR→luma conversion (edge mode) | `processing/edge_mask.cpp` | 3 multiplies, 2 adds, 1 shift |
| Sobel gradient magnitude | `processing/edge_mask.cpp` | 18 adds/subs, 2 abs, 1 add |
| Colour histogram (auto-sensitivity) | `processing/auto_sensitivity.cpp` | 3 shifts, index compute, increment |
| Mask invert + copy-with-mask | `processing/foreground.cpp` | 1–3 ops per pixel |

At 1920×1080 these loops process ~2 M pixels per call. With scalar code each pixel is processed one at a time; AVX2 can process 32 bytes (32 uint8 values or 8 floats) per instruction, giving a theoretical 8–32× throughput gain on vectorisable loops.

`std::simd` (C++26 / P1928) is not yet available in MSVC. Raw intrinsics were considered but rejected because they would make the loops unreadable and hard to maintain. A portable wrapper library was preferred.

---

## Decision

Use **xsimd** (header-only, Apache 2.0) as the SIMD abstraction layer.

- Added to `vcpkg.json` as a versioned dependency (`>= 11.0.0`)
- Linked to `CountVonCountLib` via `target_link_libraries`
- MSVC target receives `/arch:AVX2` so xsimd selects 256-bit (`xsimd::avx2`) at compile time

xsimd was chosen over the alternatives because:
- Header-only, zero runtime overhead, installs through vcpkg with no extra setup
- API maps directly onto the pointer-based row access pattern used by `cc::Image`
- Proven MSVC support; batch types (`xsimd::batch<uint8_t, xsimd::avx2>`) cover the byte-level pixel work that dominates this pipeline
- No runtime dispatch complexity needed — the deployment target is a fixed workstation

---

## Planned implementation

Prioritised by expected speedup × implementation simplicity:

### 1. Chebyshev foreground threshold (`foreground.cpp`) — highest priority

The innermost loop is a textbook SIMD candidate: three independent abs-diffs, a max-reduction, and a compare, all on `uint8_t` data laid out contiguously.

```
// Scalar (current):
for each pixel:
    dist = max(|src.b - ref_b|, |src.g - ref_g|, |src.r - ref_r|)
    mask[x] = dist <= tolerance ? 255 : 0

// SIMD target (xsimd, AVX2, 32 pixels per iteration):
using batch_u8 = xsimd::batch<uint8_t, xsimd::avx2>;  // 32 lanes
for each 32-pixel chunk:
    load 96 bytes of BGR → deinterleave into b, g, r batches
    dist = xsimd::max(abs(b - ref_b), xsimd::max(abs(g - ref_g), abs(r - ref_r)))
    store (dist <= tol) ? 0xFF : 0x00 → mask row
handle tail (< 32 pixels) with scalar fallback
```

Deinterleaving RGB is the only non-trivial part; xsimd does not have a built-in deinterleave for 3-channel data, so a manual gather using two `batch::load_unaligned` + shuffle or a scalar pre-pass for the tail is required.

### 2. BGR→luma conversion (`edge_mask.cpp`)

Fixed-point multiply-and-shift on `uint8_t` input, `uint8_t` output. Straightforward 16-bit intermediate (`xsimd::batch<uint16_t>`), 32 pixels per iteration.

```
// luma = (r*77 + g*150 + b*29) >> 8
// Process 16 pixels at once using uint16_t batch to avoid overflow:
load b, g, r as uint8 → widen to uint16
result = (r*77 + g*150 + b*29) >> 8
narrow back to uint8, store
```

### 3. Mask invert and copy-with-mask (`foreground.cpp`)

Simple bitwise NOT on the mask (invert) and a branchless select for the copy. Both are single-instruction per 32-byte chunk with `xsimd::select` or `xsimd::bitwise_andnot`.

### 4. Sobel gradient (`edge_mask.cpp`) — lower priority

The stencil access pattern (reads from three rows simultaneously) does not vectorise as cleanly as the above. Each output pixel requires 8 input reads from non-contiguous positions. A sliding-window approach using `xsimd::load_unaligned` with per-row offsets is feasible but the implementation complexity is higher relative to gain.

### 5. Colour histogram (`auto_sensitivity.cpp`) — low priority

Histogram accumulation has a data dependency (read-modify-write into `histogram[bin]`) that prevents straightforward SIMD parallelism. A scatter-gather or sub-histogram reduction approach (4 independent histograms merged at the end) is possible but not a priority given that auto-sensitivity runs once per keypress, not every frame.

---

## Implementation rules

1. **No SIMD in headers or templates.** All SIMD code lives in `.cpp` files so the xsimd include is not transitive into the test build unless explicitly needed.
2. **Scalar fallback for all tail loops.** Every SIMD loop must handle the `n % batch_size` remainder with the existing scalar code path.
3. **Correctness first.** The scalar implementation stays in place behind a compile-time or runtime flag during development. SIMD paths are added alongside, not replacing, until tests pass.
4. **Tests stay scalar.** Unit tests in `tests/` do not include xsimd headers and do not depend on `/arch:AVX2`. The observable outputs (pixel values, mask contents) must be bit-identical to the scalar path.
5. **One loop at a time.** Each vectorised loop is a self-contained PR so regressions are easy to isolate.

---

## Consequences

**Positive:**
- Expected 4–16× throughput on the three high-priority loops (net frame-time reduction depends on the fraction of time spent in these loops vs. contour tracing, which is inherently serial)
- xsimd API is close enough to `std::simd` that migrating when MSVC ships C++26 support will be mostly mechanical find-and-replace
- `/arch:AVX2` only applies to `CountVonCountLib`; the test binary is unaffected

**Negative / risks:**
- `/arch:AVX2` raises the minimum CPU requirement (AVX2 requires Haswell 2013 or later — acceptable for the deployment context)
- RGB deinterleaving adds non-obvious code; must be clearly documented and tested at the pixel level
- Sobel vectorisation is deferred; edge detection mode remains scalar for now
