# ADR-004: Radial Distance Thresholding

**Date:** 2026-05-16  
**Status:** Proposed — preferred option identified

---

## Context

After assembling the radial distance profile `distances[]` (one entry per contour point), the pipeline binarises it to produce `tooth_mask[]`: a 1 at each point whose distance exceeds the threshold, 0 elsewhere. The binary mask is the direct input to `count_teeth()` which counts rising-edge transitions.

The current threshold is computed by `cc::math::otsu_threshold(distances)`, a global Otsu applied to the full distance vector.

Otsu's method finds the threshold that maximises between-class variance. It is optimal when the value distribution is bimodal with two well-separated, roughly equal-weight peaks. For a correctly segmented gear with N teeth, the distance histogram should show:

- A "gap" peak centred at `r_gap` (contour points lying between teeth).
- A "tooth-tip" peak centred at `r_tip` (contour points on tooth tips).

The diagnostic output on the reference image reports only 9 rising edges despite the gear having 72 teeth. Two failure modes can cause this:

1. **Unimodal distribution**: if the foreground blur (ADR-002) bridged the tooth gaps, the contour follows a smooth nearly-circular outline. The entire distance profile is close to one value (`r_tip`), with very little spread. Otsu on a unimodal distribution places the threshold unpredictably — often near one tail, causing nearly all points to be classified as "tooth" or nearly all as "gap", yielding 0 or a very small number of transitions.

2. **Off-centre centroid**: the centroid is the arithmetic mean of contour points. On a heavily blurred mask the contour may be non-circular (asymmetric partial capture), shifting the centroid away from the geometric centre. This introduces a sinusoidal low-frequency component into `distances[]` that can dominate the bimodal tooth signal, smearing the two peaks into a single broad hump.

Both failure modes cause the direct tooth counter to undercount severely. The FFT path (`fft_tooth_count`) is more resilient because it looks at the *frequency content* of the profile rather than a binary threshold, but it too is affected when the profile is distorted.

---

## Alternatives

### A — Otsu on smoothed profile *(preferred)*

Apply the circular moving average from ADR-003 to `distances[]` before computing Otsu. A smoothed profile has sharper, taller peaks in the histogram, widening the valley between the two Otsu classes and improving threshold placement.

If the profile swing (`dist_max − dist_min`) is below a minimum floor (e.g., 2 px), fall back to a fixed percentile midpoint threshold rather than Otsu, to prevent threshold collapse when the profile is nearly flat.

```cpp
// In process_contours(), after smoothing distances[]:
double swing = dist_max - dist_min;
double threshold;
if (swing < k_MinSwingPx)
    threshold = dist_min + swing * 0.5;  // midpoint fallback
else
    threshold = cc::math::otsu_threshold(distances);
```

**Pro:** Minimal code change; directly addresses the contaminated input that makes Otsu fail. The smoothing is already being added by ADR-003, so this adds only the swing-check and fallback.  
**Con:** A flat profile (swing < floor) could be genuine (a ring gear with no radial tooth variation), where any threshold would fail. The fallback correctly returns all-tooth or all-gap in that case, and `count_teeth` would then return zero, which is handled by the `>= k_MinimumToothCount` guard.

---

### B — Percentile threshold

Replace Otsu with the existing `cc::math::percentile_threshold()` (midpoint of P25 and P75).

**Pro:** Already implemented and tested; robust against a few large outliers.  
**Con:** P25/P75 midpoint is not always the best separator. For a 72-tooth gear where roughly 50 % of contour points are on tooth tips and 50 % are on gaps, P50 (the median) and the Otsu threshold should coincide. If the duty cycle is not 50 %, the P25/P75 midpoint may be biased toward the more common class.

---

### C — Local adaptive threshold (sliding angular window)

Apply Otsu within a sliding angular window of width ≈ 10° (covering ~2 teeth). Each point is classified relative to its local neighbourhood rather than the global distribution.

**Pro:** Handles gradually varying radius (e.g., slight eccentricity or oval gears) that a global threshold cannot accommodate.  
**Con:** Significant implementation complexity. The window must be circular (wrap-around). Window size in samples depends on tooth pitch, which is not known until after counting. Circular dependency similar to ADR-002 option C.

---

### D — Peak detection on the distance profile (bypass binary threshold entirely)

Instead of a hard threshold, find all local maxima in the smoothed distance profile. Each maximum is a tooth tip; count them directly.

**Pro:** Eliminates the binary-threshold step altogether; the direct count becomes the same as the FFT estimate in theory.  
**Con:** Local maxima detection is sensitive to smoothing window and requires a prominence filter (otherwise noise spikes become teeth). The span filter in `count_teeth()` already does something similar for the binary path; duplicating that logic in a peak-finder adds maintenance burden.

---

## Decision (preferred)

**Option A — Otsu on the smoothed profile from ADR-003, with a minimum-swing fallback to midpoint.**

Smoothing the profile before thresholding is the correct first step because the underlying histogram is still bimodal once the profile is clean — Otsu just needs a better input. The swing check costs one subtraction and prevents degenerate behaviour on flat profiles, which would otherwise produce an unpredictable threshold.

The constant `k_MinSwingPx` should be defined as a named constant in `contours.cpp`:

```cpp
constexpr double k_MinSwingPx = 3.0;
```

This corresponds to a total tooth-tip excursion of 3 px being the minimum required for meaningful threshold discrimination.

---

## Consequences

**Positive:**
- The direct counter should reliably find all rising edges once the profile peaks are clean.
- The fallback prevents silent failure on rings or circular objects with no teeth.
- No API changes; the change is local to `process_contours()`.

**Negative / risks:**
- The smoothed distance profile shifts peak locations slightly (by `w/2` samples, as noted in ADR-003). Otsu still finds the correct threshold because the bimodal shape is preserved; only the transition positions are offset.
- The swing threshold `k_MinSwingPx = 3.0` is heuristic. Too low and noise-induced threshold failure is not guarded; too high and shallow-tooth gears (worm gears, splines) are incorrectly rejected. The value should be validated against the actual `dist_max − dist_min` values logged in debug mode.
