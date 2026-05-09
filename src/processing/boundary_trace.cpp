#include "boundary_trace.h"

#include <cstring>
#include <algorithm>

namespace cc::processing {

    // Connected component labeling (two-pass with union-find)
    // Then Moore boundary tracing on each component

    namespace {
        // Moore neighborhood (8-connectivity), clockwise starting from right
        static constexpr int dx8[] = { 1,  1,  0, -1, -1, -1,  0,  1 };
        static constexpr int dy8[] = { 0,  1,  1,  1,  0, -1, -1, -1 };

        // Moore boundary tracing: trace the boundary of a connected component
        // starting from the given seed pixel
        std::vector<cc::Point2i> trace_boundary(
            const cc::Image& mask,
            int start_x,
            int start_y
        ) {
            int rows = mask.rows();
            int cols = mask.cols();

            std::vector<cc::Point2i> boundary;
            boundary.push_back({ start_x, start_y });

            // Find the initial backtrack direction
            // We enter from the left (the pixel to the left of start is background)
            int backtrack_dir = 4; // start looking from left

            // Check: if start_x == 0, backtrack is from left edge
            if (start_x > 0 && mask.at(start_y, start_x - 1)[0] > 0) {
                // The pixel to the left is also foreground, need to find proper start
                backtrack_dir = 4;
            }

            int cx = start_x;
            int cy = start_y;

            // Start scanning from the backtrack direction going clockwise
            for (int iter = 0; iter < rows * cols * 2; ++iter) {
                bool found = false;

                for (int i = 0; i < 8; ++i) {
                    int dir = (backtrack_dir + i) % 8;
                    int nx = cx + dx8[dir];
                    int ny = cy + dy8[dir];

                    if (nx >= 0 && nx < cols && ny >= 0 && ny < rows &&
                        mask.at(ny, nx)[0] > 0)
                    {
                        cx = nx;
                        cy = ny;
                        backtrack_dir = (dir + 5) % 8; // reverse + 1 step back
                        found = true;

                        if (cx == start_x && cy == start_y) {
                            // Back to start
                            return boundary;
                        }

                        boundary.push_back({ cx, cy });
                        break;
                    }
                }

                if (!found)
                    break; // isolated pixel
            }

            return boundary;
        }
    }

    std::vector<std::vector<cc::Point2i>> find_contours(const cc::Image& binary_mask) {
        if (binary_mask.empty() || binary_mask.channels() != 1)
            return {};

        int rows = binary_mask.rows();
        int cols = binary_mask.cols();

        // Create a visited mask to avoid tracing the same component twice
        std::vector<uint8_t> visited(static_cast<size_t>(rows) * cols, 0);

        std::vector<std::vector<cc::Point2i>> contours;

        // Scan top-to-bottom, left-to-right for unvisited foreground pixels
        // that have a background pixel to their left (or are on the left edge)
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (binary_mask.at(y, x)[0] == 0)
                    continue;

                if (visited[static_cast<size_t>(y) * cols + x])
                    continue;

                // Check if this is a boundary pixel (has at least one background neighbor
                // or is on the image edge)
                bool is_boundary = false;
                if (x == 0 || y == 0 || x == cols - 1 || y == rows - 1) {
                    is_boundary = true;
                } else {
                    for (int d = 0; d < 8; ++d) {
                        int nx = x + dx8[d];
                        int ny = y + dy8[d];
                        if (nx >= 0 && nx < cols && ny >= 0 && ny < rows &&
                            binary_mask.at(ny, nx)[0] == 0)
                        {
                            is_boundary = true;
                            break;
                        }
                    }
                }

                if (!is_boundary) {
                    visited[static_cast<size_t>(y) * cols + x] = 1;
                    continue;
                }

                // Check if the left neighbor is background (or we're on left edge)
                // This ensures we only start tracing from the leftmost pixel of each row
                // of each component (avoiding duplicate traces)
                bool left_is_bg = (x == 0) || (binary_mask.at(y, x - 1)[0] == 0);
                if (!left_is_bg) {
                    visited[static_cast<size_t>(y) * cols + x] = 1;
                    continue;
                }

                // Trace boundary
                auto boundary = trace_boundary(binary_mask, x, y);

                // Mark all boundary pixels as visited
                for (const auto& pt : boundary)
                    visited[static_cast<size_t>(pt.y) * cols + pt.x] = 1;

                // Flood-fill from the boundary to mark all connected foreground pixels as visited.
                // Mark pixels as visited before pushing to prevent duplicates in the stack.
                if (!boundary.empty()) {
                    std::vector<cc::Point2i> stack;
                    stack.reserve(boundary.size());

                    for (const auto& pt : boundary)
                        stack.push_back(pt);

                    while (!stack.empty()) {
                        auto pt = stack.back();
                        stack.pop_back();

                        for (int d = 0; d < 8; ++d) {
                            int nx = pt.x + dx8[d];
                            int ny = pt.y + dy8[d];
                            auto nidx = static_cast<size_t>(ny) * cols + nx;
                            if (nx >= 0 && nx < cols && ny >= 0 && ny < rows &&
                                !visited[nidx] &&
                                binary_mask.at(ny, nx)[0] > 0)
                            {
                                visited[nidx] = 1;
                                stack.push_back({ nx, ny });
                            }
                        }
                    }

                    contours.push_back(std::move(boundary));
                }
            }
        }

        return contours;
    }
}
