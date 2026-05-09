#include "edge_mask.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace cc::processing {
    namespace {
        // 3x3 dilate / erode on a packed 0/1 buffer. Scratch is reused.
        void dilate_3x3(std::vector<uint8_t>& img, std::vector<uint8_t>& scratch, int rows, int cols) {
            std::fill(scratch.begin(), scratch.end(), uint8_t{0});
            for (int y = 0; y < rows; ++y) {
                int y0 = std::max(0, y - 1);
                int y1 = std::min(rows - 1, y + 1);
                for (int x = 0; x < cols; ++x) {
                    int x0 = std::max(0, x - 1);
                    int x1 = std::min(cols - 1, x + 1);
                    uint8_t v = 0;
                    for (int ny = y0; ny <= y1 && !v; ++ny)
                        for (int nx = x0; nx <= x1 && !v; ++nx)
                            if (img[static_cast<size_t>(ny) * cols + nx]) v = 1;
                    scratch[static_cast<size_t>(y) * cols + x] = v;
                }
            }
            img.swap(scratch);
        }

        void erode_3x3(std::vector<uint8_t>& img, std::vector<uint8_t>& scratch, int rows, int cols) {
            std::fill(scratch.begin(), scratch.end(), uint8_t{0});
            for (int y = 0; y < rows; ++y) {
                int y0 = std::max(0, y - 1);
                int y1 = std::min(rows - 1, y + 1);
                for (int x = 0; x < cols; ++x) {
                    int x0 = std::max(0, x - 1);
                    int x1 = std::min(cols - 1, x + 1);
                    uint8_t v = 1;
                    for (int ny = y0; ny <= y1 && v; ++ny)
                        for (int nx = x0; nx <= x1 && v; ++nx)
                            if (!img[static_cast<size_t>(ny) * cols + nx]) v = 0;
                    scratch[static_cast<size_t>(y) * cols + x] = v;
                }
            }
            img.swap(scratch);
        }
    }

    void determine_foreground_by_edges(
              int         sensitivity,
        const cc::Image&  source_image,
              cc::Image&  foreground_mask,
              cc::Image&  foreground
    ) {
        const int rows = source_image.rows();
        const int cols = source_image.cols();

        if (source_image.channels() != 3 || rows < 3 || cols < 3)
            return;

        const size_t n = static_cast<size_t>(rows) * cols;

        // Step 1: BGR → luma (ITU BT.601 approx, fixed-point)
        std::vector<uint8_t> gray(n, 0);
        for (int y = 0; y < rows; ++y) {
            const uint8_t* src = source_image.ptr(y);
            uint8_t*       dst = gray.data() + static_cast<size_t>(y) * cols;
            for (int x = 0; x < cols; ++x) {
                int b = src[x * 3 + 0];
                int g = src[x * 3 + 1];
                int r = src[x * 3 + 2];
                dst[x] = static_cast<uint8_t>((r * 77 + g * 150 + b * 29) >> 8);
            }
        }

        // Step 2: Sobel magnitude (L1 approximation: |Gx| + |Gy|)
        std::vector<uint16_t> mag(n, 0);
        double sum_mag    = 0.0;
        double sum_mag_sq = 0.0;
        int    interior_px = 0;

        for (int y = 1; y < rows - 1; ++y) {
            const uint8_t* r_prev = gray.data() + static_cast<size_t>(y - 1) * cols;
            const uint8_t* r_curr = gray.data() + static_cast<size_t>(y    ) * cols;
            const uint8_t* r_next = gray.data() + static_cast<size_t>(y + 1) * cols;
            uint16_t* m_row = mag.data() + static_cast<size_t>(y) * cols;

            for (int x = 1; x < cols - 1; ++x) {
                int gx = -r_prev[x - 1] - 2 * r_curr[x - 1] - r_next[x - 1]
                       +  r_prev[x + 1] + 2 * r_curr[x + 1] + r_next[x + 1];
                int gy = -r_prev[x - 1] - 2 * r_prev[x] - r_prev[x + 1]
                       +  r_next[x - 1] + 2 * r_next[x] + r_next[x + 1];
                int m = std::abs(gx) + std::abs(gy);
                m_row[x] = static_cast<uint16_t>(std::min(m, 65535));
                sum_mag    += m;
                sum_mag_sq += static_cast<double>(m) * m;
                ++interior_px;
            }
        }

        // Step 3: statistical threshold = mean + k·stddev of the magnitude distribution.
        // Higher sensitivity → lower k → more edges pass. Using strict `>` below so a
        // zero threshold on a flat image cannot flag zero-magnitude pixels as edges.
        double threshold = 0.0;
        if (interior_px > 0) {
            double mean     = sum_mag / interior_px;
            double variance = std::max(0.0, (sum_mag_sq / interior_px) - mean * mean);
            double stddev   = std::sqrt(variance);

            int    clamped_sensitivity = std::clamp(sensitivity, 0, 100);
            double k = 3.0 - (clamped_sensitivity / 100.0) * 2.7; // k ∈ [0.3, 3.0]
            threshold = mean + k * stddev;
        }

        // Step 4: binarise (strict > so threshold == 0 still rejects flat regions)
        std::vector<uint8_t> edges(n, 0);
        for (size_t i = 0; i < n; ++i)
            edges[i] = (static_cast<double>(mag[i]) > threshold) ? 1 : 0;

        // Step 5: morphological close (2x dilate + 2x erode ≈ 5x5 close)
        std::vector<uint8_t> scratch(n, 0);
        dilate_3x3(edges, scratch, rows, cols);
        dilate_3x3(edges, scratch, rows, cols);
        erode_3x3 (edges, scratch, rows, cols);
        erode_3x3 (edges, scratch, rows, cols);

        // Step 6: flood-fill non-edge pixels from the image border.
        // visited[i] == 1 means "reachable from border through non-edge pixels" = exterior.
        // Everything not visited and not an edge is "inside a closed contour" = the gear body.
        std::vector<uint8_t> visited(n, 0);
        std::vector<int> stack;
        stack.reserve(n / 4);

        auto try_push = [&](int y, int x) {
            size_t i = static_cast<size_t>(y) * cols + x;
            if (!visited[i] && !edges[i]) {
                visited[i] = 1;
                stack.push_back(static_cast<int>(i));
            }
        };

        for (int x = 0; x < cols; ++x) {
            try_push(0, x);
            try_push(rows - 1, x);
        }
        for (int y = 0; y < rows; ++y) {
            try_push(y, 0);
            try_push(y, cols - 1);
        }

        while (!stack.empty()) {
            int i = stack.back();
            stack.pop_back();
            int y = i / cols;
            int x = i % cols;
            if (y > 0)        try_push(y - 1, x);
            if (y < rows - 1) try_push(y + 1, x);
            if (x > 0)        try_push(y, x - 1);
            if (x < cols - 1) try_push(y, x + 1);
        }

        // Step 7: write foreground_mask (255 inside, 0 outside). Edge pixels count as
        // inside so the contour trace sees a solid blob rather than a ring.
        std::memset(foreground_mask.data(), 0, foreground_mask.total_bytes());
        for (int y = 0; y < rows; ++y) {
            uint8_t* mask_row = foreground_mask.ptr(y);
            const uint8_t* vis_row = visited.data() + static_cast<size_t>(y) * cols;
            for (int x = 0; x < cols; ++x) {
                if (!vis_row[x])
                    mask_row[x] = 255;
            }
        }

        // Step 8: masked source copy for the "foreground" view
        std::memset(foreground.data(), 0, foreground.total_bytes());
        for (int y = 0; y < rows; ++y) {
            const uint8_t* src_row  = source_image.ptr(y);
            const uint8_t* mask_row = foreground_mask.ptr(y);
            uint8_t*       dst_row  = foreground.ptr(y);
            for (int x = 0; x < cols; ++x) {
                if (mask_row[x]) {
                    dst_row[x * 3 + 0] = src_row[x * 3 + 0];
                    dst_row[x * 3 + 1] = src_row[x * 3 + 1];
                    dst_row[x * 3 + 2] = src_row[x * 3 + 2];
                }
            }
        }
    }
}
