# ADR-002: Foreground Segmentation

**Date:** 2026-05-16  
**Status:** Proposed — preferred option identified

---

## Context

The current foreground pipeline runs three steps in order:

1. **Chebyshev hard threshold** (`foreground.cpp → chebyshev_threshold()`): for every pixel compute `max(|ΔB|, |ΔG|, |ΔR|)` against the auto-detected background colour; write 255 if the distance is ≤ tolerance, else 0.
2. **9×9 majority-vote blur** (`foreground.cpp → majority_vote_blur()`): replace each mask pixel with 255 if more than half of its 9×9 neighbourhood is already 255, else 0. Implemented with an integral image and AVX2 SIMD.
3. **Mask invert** (`invert_mask()`): flip 255 ↔ 0 so that gear pixels become the foreground.

On a 72-tooth gear the measured result is 9 direct teeth and 67 speculative (FFT). The expected result is 72.

The root cause of the low direct count is that the 9×9 kernel bridges across narrow inter-tooth gaps. For a 72-tooth gear at typical resolution (~640 px outer diameter), the circumference is roughly 2 010 px, giving a tooth pitch of ~28 px and a gap width of ~14 px. A 9×9 window centred on a gap pixel has 81 neighbourhood pixels. If the two flanking tooth tips each contribute ~4 px columns of foreground, those 8 columns alone can push the majority count above 40, causing the gap to be voted foreground. The gaps disappear from the mask before contour tracing begins, so Moore-tracing of the blurred mask sees a nearly-smooth gear outline rather than discrete teeth.

---

## Alternatives

### A — Reduce kernel size to 3×3

Change the hard-coded `half = 4` constant in `majority_vote_blur()` to `half = 1`.

**Pro:** Zero architectural change; single-constant edit.  
**Con:** A 3×3 kernel suppresses very little noise. Small isolated foreground blobs (camera sensor noise, JPEG compression artefacts) survive unfiltered and become spurious contour candidates. Threshold sensitivity increases.

---

### B — Replace blur with morphological open (erode then dilate) *(preferred)*

Remove `majority_vote_blur()` from the pipeline. Instead:

1. **Erode** with a small structuring element (e.g., 3×3 disk): removes isolated foreground pixels and thins the mask everywhere.
2. **Dilate** with the same element: restores regions that were genuinely foreground while keeping narrow gaps open.

The net effect of open = erode + dilate is to remove thin noise spikes while preserving connected foreground regions whose width exceeds the kernel radius. For a 14 px gap, a 3×3 kernel is narrow enough to leave the gap open but wide enough to erase single-pixel noise.

`processing/morphology.h/.cpp` already implements `dilate()`, `erode()`, and `morphological_close()` (confirmed by `tests/test_morphology.cpp`). The new pipeline would call `erode()` then `dilate()` using the existing API at negligible extra implementation cost.

**Pro:** Correctly preserves inter-tooth gaps regardless of tooth count; uses code that already exists and is tested.  
**Con:** No AVX2 path for erode/dilate yet (majority-vote blur is SIMD-accelerated); morphological open adds two passes over the mask instead of one. For a 640×480 mask these two passes complete in well under 1 ms, so the performance regression is acceptable.

---

### C — Make kernel size proportional to estimated tooth pitch

Estimate the tooth pitch from the first FFT speculative count and scale the blur kernel to ¼ of the pitch. Larger kernels for coarse gears, smaller for fine-pitch gears.

**Pro:** Self-adapts across gear sizes.  
**Con:** Circular dependency: the FFT runs after segmentation, so the pitch used to configure segmentation is from the previous frame. On the first frame the estimate is unavailable. Adds a stateful feedback loop that is fragile during cold start and gear changes.

---

### D — Two-pass: small blur + morphological erosion

Keep a reduced blur (5×5) for noise suppression and add a morphological erosion step immediately after to re-open gaps that the blur might have closed.

**Pro:** Retains SIMD performance of the blur for noise suppression; erosion is a cheap second pass.  
**Con:** Two tunable parameters (blur kernel, erosion kernel) with coupled effects; harder to reason about than a single morphological open.

---

## Decision (preferred)

**Option B — morphological open (erode 3×3 + dilate 3×3) replacing the 9×9 majority-vote blur.**

The majority-vote blur was introduced to fill small holes inside the gear body and suppress noise, not to bridge gaps between teeth. A morphological open achieves the noise-suppression goal with a structural guarantee: gaps narrower than the structuring element remain open. The 3×3 disk is the smallest element that eliminates single-pixel noise while preserving 14 px tooth gaps with ample margin.

Implementation sketch:

```cpp
// In determine_foreground(), replace:
majority_vote_blur(foreground_mask, blur_temp);

// With:
erode (foreground_mask, blur_temp, 1);   // 3×3 disk, radius=1
dilate(foreground_mask, blur_temp, 1);
```

The `blur_temp` scratch buffer already allocated by `Application` can serve as the intermediate for both passes without new allocations.

---

## Consequences

**Positive:**
- Tooth gaps are preserved in the mask, enabling the direct counter to find all rising edges.
- Noise suppression is retained (isolated single-pixel blobs are removed by the erosion step).
- No new code; only re-uses tested morphology primitives.

**Negative / risks:**
- If the camera is noisy or compression artefacts are large, a 3×3 open may not suppress all spurious blobs. A 5×5 open can be tried if needed.
- The SIMD speedup of `majority_vote_blur` (2.2× measured) is lost; the morphological passes are scalar for now. For a 640×480 mask both passes fit comfortably within a frame budget.
- The erode step may thin the gear body mask slightly, shrinking contour area. The 1% area threshold in `process_contours` should still pass for any realistic gear.
