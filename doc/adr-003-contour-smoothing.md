# ADR-003: Contour Resampling and Smoothing

**Date:** 2026-05-16  
**Status:** Proposed — preferred option identified

---

## Context

Moore boundary tracing (`processing/boundary_trace`) produces a contour as a sequence of integer pixel coordinates. All downstream tooth-counting logic derives from the radial distance profile: for each contour point `p`, compute `dist(p, centroid)` and store it in `distances[]`.

Integer pixel coordinates introduce two aliasing artefacts in the distance profile:

1. **Staircase quantisation**: diagonal Moore steps (±1, ±1) have a Euclidean length of √2 ≈ 1.414 px, while axis-aligned steps have length 1. The true arc length is uneven, so angular coverage per sample varies by up to 41 %. When the profile is sorted by `atan2` angle and resampled (as the FFT path does), interpolation between widely-spaced diagonal samples is linear and introduces smooth low-frequency errors.

2. **Per-point distance noise**: a pixel on the boundary of a tooth tip may be placed up to ~0.7 px from the true smooth curve, giving a distance error of the same magnitude relative to the centroid. For a 320 px-radius gear with 72 teeth, tooth amplitude is approximately 10–15 px. A ±0.7 px per-sample error is 5–7 % of the signal amplitude — enough to pull Otsu's threshold out of the valley between the two distance distributions, especially after adjacent values happen to form a run of high-or-low errors.

These errors accumulate before reaching either the Otsu threshold step (ADR-004) or the FFT step (ADR-005), so improving the profile quality here benefits both downstream consumers simultaneously.

The FFT already resamples to 1 024 uniform angular bins via linear interpolation. The direct counter, however, operates on the raw `distances[]` array, meaning Otsu sees the un-smoothed, un-resampled profile.

---

## Alternatives

### A — 2-D contour smoothing (moving average over point coordinates)

Before computing distances, replace each contour point with the centroid of its `k`-nearest neighbours along the contour (circular moving average in 2-D).

**Pro:** Smoothes both x and y jointly; well-understood.  
**Con:** The smoothed points no longer lie on the original pixel boundary, complicating any code that uses contour points for drawing. The benefit in the distance profile is indirect and depends on `k`.

---

### B — 1-D radial distance profile smoothing *(preferred)*

After computing `distances[]` from integer contour coordinates, apply a small circular moving average directly to the 1-D distance vector before passing it to Otsu and `count_teeth`.

A window of `w` samples at `n` total contour points averages over a circular arc of `2π·w/n` radians. For `n = 3 000` contour points and `w = 11`, this averages over about 1.3° — roughly one-quarter of the inter-tooth spacing on a 72-tooth gear (5°). That is narrow enough to preserve the tooth signal while suppressing per-sample quantisation noise.

`math/` already contains a `circular_moving_average` (confirmed by `tests/test_smooth.cpp`), so implementation is a one-line call.

```cpp
// In process_contours(), after distances[] is populated:
distances = cc::math::circular_moving_average(distances, /*window=*/11);
```

**Pro:** Directly improves the inputs to both Otsu (direct count) and FFT; uses existing tested utility; no change to contour representation or drawing.  
**Con:** A window that is too wide (> ½ tooth pitch in samples) will round off tooth-tip peaks and reduce the amplitude of the distance swing, making Otsu's job harder. Window size must be validated experimentally.

---

### C — Resample contour to uniform arc-length with subpixel interpolation

Compute cumulative arc length along the contour, then interpolate (bilinear or cubic) to produce `M` evenly-spaced samples at subpixel positions.

**Pro:** Eliminates staircase artefacts entirely; gives the FFT a clean, uniformly-sampled signal without the angular-sort step it currently needs.  
**Con:** Requires interpolation into image space (not just distance space), which is significantly more complex. The FFT already handles non-uniform angles via sort-and-resample, so this would duplicate that mechanism at a higher level.

---

### D — Fit a parametric curve (spline or ellipse) and sample it

Fit a closed spline or ellipse to the contour, then evaluate it at `M` uniform angles to produce a smooth profile.

**Pro:** Maximum smoothness; principled subpixel accuracy.  
**Con:** Fitting a spline with 3 000 control points is expensive. Ellipse fitting would remove all non-elliptical shape (i.e., the teeth themselves) from the profile, defeating the purpose.

---

## Decision (preferred)

**Option B — 1-D circular moving average on `distances[]`, window = 11 samples.**

The fix is a single call to an already-tested utility applied at the one place in the pipeline where distances are first assembled (`process_contours()` in `contours.cpp`). A window of 11 keeps the averaging arc below one tooth pitch for any gear with more than ~8 teeth (safely below the `k_MinimumToothCount = 8` guard in `application.h`), so no special-casing is needed.

The window parameter should be exposed as a named constant rather than a magic number so it can be tuned later without searching call sites:

```cpp
// contours.cpp (file-scope anonymous namespace)
constexpr int k_DistanceSmoothWindow = 11;
```

---

## Consequences

**Positive:**
- Otsu sees smoother distance distributions with better-separated tooth-tip and gap peaks.
- FFT input quality improves for the angular resampling step at no extra cost (the FFT already re-sorts by angle; it benefits from a cleaner distance value at each sample).
- Zero new dependencies; no changes to public APIs.

**Negative / risks:**
- A window too wide relative to tooth pitch will flatten the tooth-tip peaks, reducing the Otsu gap. If teeth are very closely spaced (> 200 teeth at the current contour resolution) the default window may need to be reduced.
- The smoothing slightly delays the rise and fall edges of each tooth, shifting the measured transition indices by ≤ `w/2` samples. This affects `m_LowHighTransitionIdx` and `m_HighLowTransitionIdx` values in `ToothMeasurement`. Anomaly detection compares teeth against each other, so a uniform shift has no net effect on relative measurements.
