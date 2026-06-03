#include "white_balance.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cc::processing {
    double color_imbalance(const cc::Image& bgr, int subsample) {
        if (bgr.empty() || bgr.channels() != 3)
            return 0.0;
        if (subsample < 1) subsample = 1;

        std::array<long, 256> hb{}, hg{}, hr{};
        long n = 0;
        for (int y = 0; y < bgr.rows(); y += subsample) {
            const uint8_t* row = bgr.ptr(y);
            for (int x = 0; x < bgr.cols(); x += subsample) {
                const uint8_t* p = row + static_cast<size_t>(x) * 3;
                ++hb[p[0]];
                ++hg[p[1]];
                ++hr[p[2]];
                ++n;
            }
        }
        if (n == 0)
            return 0.0;

        auto median = [&](const std::array<long, 256>& h) {
            long cum = 0;
            for (int i = 0; i < 256; ++i) {
                cum += h[static_cast<size_t>(i)];
                if (cum * 2 >= n) return i;
            }
            return 255;
        };

        int mb = median(hb), mg = median(hg), mr = median(hr);
        return static_cast<double>(std::abs(mr - mg) + std::abs(mg - mb));
    }
}
