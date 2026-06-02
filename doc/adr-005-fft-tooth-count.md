# ADR-005: FFT Tooth Count Estimation

**Date:** 2026-05-16  
**Status:** Proposed — preferred option identified

---

## Context

`fft_tooth_count()` in `processing/count_teeth.cpp` estimates the number of gear teeth from the raw contour geometry without relying on a binary threshold. The steps are:

1. Compute `(angle, distance)` pairs for every contour point via `atan2`.
2. Sort by angle and resample to 1 024 uniform angular bins via linear interpolation.
3. Run a Cooley-Tukey radix-2 FFT on the resampled signal.
4. Scan bins 1 … 512, convert each bin `k` to a tooth count via `round(k · 2π / a_range)`, and return the bin with maximum power.

On the reference image (72 actual teeth) the function returns **67**, an error of 7 %. The direct counter finds only 9, so the FFT is the better of the two paths, but still misses by five teeth.

There are several plausible sources of the 7 % error, and they can compound:

| Source | Mechanism | Typical magnitude |
|---|---|---|
| DC component | If the centroid is slightly off-centre, `distances[]` has a non-zero mean. The DC bin (bin 0) soaks up power, but through spectral leakage it also raises the floor near bin 0, biasing the peak-finder toward lower frequencies. | Several bins of leakage |
| Rectangular window leakage | The resampled signal is implicitly windowed by a rectangular (boxcar) function. Frequency-domain leakage spreads power from the true tooth frequency into adjacent bins. If a sub-harmonic bin (e.g., 36 teeth) has more leaked power than the true bin (72 teeth), it wins. | ±1–3 bins |
| Angular range < 2π | The code computes `a_range = a_end − a_start` from `atan2` output. For a closed contour `a_range` is typically 2π − ε. The rounding in `round(k · 2π / a_range)` can misplace the peak by 1–2 teeth at k ≈ 72 when ε is non-negligible. | ±1–2 teeth |
| Sub-harmonic dominance | Square-wave profiles (sharp tooth-tip plateau, sharp gap) have strong harmonics at 3k, 5k, … and their sub-harmonics can coincide with the tooth-count bin. If blur smoothed the profile into a sinusoid at 36 oscillations rather than 72, the 36-tooth bin wins outright. | Factor of 2 error |

The observed error (67 vs. 72, ~7 %) is consistent with leakage shifting the dominant bin by ~5 rather than a factor-of-2 sub-harmonic error.

---

## Alternatives

### A — Zero-mean the distance profile

Before the FFT, subtract the mean of the resampled signal. This removes the DC component entirely, eliminating its spectral leakage into neighbouring bins.

```cpp
double mean = std::accumulate(signal.begin(), signal.end(), 0.0,
    [](double s, const auto& c){ return s + c.real(); }) / k_FFT_N;
for (auto& c : signal) c -= mean;
```

**Pro:** One-liner; eliminates a known leakage source.  
**Con:** Alone, it does not fix the rectangular window leakage from the other bins.

---

### B — Apply a Hann window before the FFT

Multiply the resampled real signal by a Hann (raised cosine) window before calling `fft_inplace`. The Hann window reduces sidelobe amplitude by ~32 dB compared to rectangular, at the cost of a 50 % wider main lobe.

```cpp
for (size_t k = 0; k < k_FFT_N; ++k) {
    double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * k / (k_FFT_N - 1)));
    signal[k] = signal[k].real() * w;
}
```

**Pro:** Standard signal-processing technique; strongly suppresses leakage.  
**Con:** Widens the spectral peak by ~2 bins; for a tooth count of 72 and 1 024 bins, bin spacing is about 0.44 teeth, so 2-bin main-lobe widening is ~0.9 teeth — acceptable. The 3 dB gain reduction at peak must be accounted for when comparing power across bins if other changes modify the power levels.

---

### C — Harmonic Product Spectrum (HPS)

After computing the magnitude spectrum `|X[k]|`, compute the HPS:

```
HPS[k] = |X[k]| × |X[2k]| × |X[3k]|
```

The true fundamental frequency `f` is the one for which `f`, `2f`, and `3f` all have significant power simultaneously. A sub-harmonic of `f` (e.g., `f/2`) will score low because `f/2 × 2 = f` may be strong, but `f/2 × 3 = 3f/2` is likely weak.

**Pro:** Directly combats sub-harmonic confusion. Commonly used in musical pitch detection, which faces exactly the same harmonic dominance problem as gear tooth counting.  
**Con:** Halves the usable frequency range (bin `k_FFT_N/2` maps to count N/2 at most before the doubled index goes out of bounds). For `k_FFT_N = 1 024` the maximum tooth count via HPS with 3 harmonics is 170, which covers all realistic gear sizes.

---

### D — Autocorrelation

Compute the circular autocorrelation of the distance profile (FFT-based: `IFFT(|FFT(x)|²)`). The first significant peak of the autocorrelation at lag `τ` gives the tooth pitch in samples; the tooth count is `k_FFT_N / τ`.

**Pro:** Naturally finds the fundamental period without harmonic confusion; well-established for periodic signals.  
**Con:** The autocorrelation peak-picker requires a prominence threshold to distinguish the first tooth-pitch lag from the zero-lag peak and from noise lags. This adds a tunable parameter. The FFT and autocorrelation carry equivalent information; HPS is a lighter-weight way to extract the fundamental.

---

### E — Combine: zero-mean + Hann window + HPS *(preferred)*

Apply all three improvements in sequence:

1. Zero-mean the resampled `signal[]` (removes DC leak).
2. Apply a Hann window (suppresses sidelobe leakage).
3. After `fft_inplace`, compute HPS using harmonics 1 × 2 × 3.
4. Pick the bin with maximum HPS value in the range `[k_MinTeeth, k_MaxTeeth / 3]`.

Each step addresses a distinct failure mode. Together they attack DC leakage, window leakage, and sub-harmonic dominance simultaneously.

Implementation is additive — the existing FFT infrastructure and angle-sorting/resampling remain unchanged:

```cpp
// 1. Zero-mean
double dc = 0.0;
for (const auto& c : signal) dc += c.real();
dc /= static_cast<double>(k_FFT_N);
for (auto& c : signal) c -= dc;

// 2. Hann window
for (size_t k = 0; k < k_FFT_N; ++k) {
    double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * k / (k_FFT_N - 1)));
    signal[k] = signal[k].real() * w;
}

fft_inplace(signal);

// 3. Magnitude spectrum
std::vector<double> mag(k_FFT_N / 2 + 1);
for (size_t k = 0; k <= k_FFT_N / 2; ++k)
    mag[k] = std::abs(signal[k]);

// 4. HPS and peak pick
int best_count = -1;
double best_hps = 0.0;
for (size_t k = 1; 3 * k <= k_FFT_N / 2; ++k) {
    int count = static_cast<int>(
        std::round(static_cast<double>(k) * 2.0 * std::numbers::pi / a_range));
    if (count < k_MinTeeth || count > k_MaxTeeth) continue;
    double hps = mag[k] * mag[2 * k] * mag[3 * k];
    if (hps > best_hps) { best_hps = hps; best_count = count; }
}
```

**Pro:** Addresses all identified error sources; each component is a small, self-contained code block; no new dependencies.  
**Con:** HPS limits `k_MaxTeeth` to one-third of `k_FFT_N / 2` = 170 effective maximum for 3-harmonic HPS. The current `k_MaxTeeth = 500` constant would need to be noted in a comment but is not a practical regression (no gear has 500 teeth at webcam resolution).

---

## Decision (preferred)

**Option E — zero-mean + Hann window + Harmonic Product Spectrum.**

The 7 % error (67 vs. 72 teeth) on the reference image is most likely spectral leakage shifting the dominant bin. Zero-mean and Hann together eliminate the two main leakage sources with a few lines of code. HPS insures against the case where blurred tooth profiles reduce the effective spatial frequency to a sub-harmonic of the true count.

The three changes are sequenced before the existing `fft_inplace` call and do not alter the function signature, return type, or any caller.

---

## Consequences

**Positive:**
- Speculative count should converge on 72 (or very close) for the reference image.
- HPS is fundamentally more correct for gear tooth counting than single-peak selection: a gear with N teeth always has energy at N and its harmonics; picking the product favours the true fundamental.
- Zero-mean removes centroid-offset sensitivity, making the estimate more robust to imperfect foreground segmentation.

**Negative / risks:**
- HPS with 3 harmonics caps the detectable tooth count at `floor(k_FFT_N / 6)` = 170. This is sufficient for all gears the application is expected to encounter, but should be documented as a constraint.
- The Hann window introduces a 50 % wider spectral main lobe. For very low tooth counts (< 8) adjacent bins could merge; the `k_MinTeeth = 4` guard already excludes the problematic low end.
- The existing FFT tests (`fft_tooth_count - sinusoidal radial profile`, `fft_tooth_count - square-wave radial profile`) cover 24, 48, and 16 teeth. They should continue to pass after the change; if not, the HPS power product for those tooth counts must be verified to still peak at the correct bin.
