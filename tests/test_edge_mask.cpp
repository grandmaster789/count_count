#include <catch2/catch_test_macros.hpp>

#include "processing/edge_mask.h"
#include "processing/boundary_trace.h"
#include "processing/contours.h"
#include "types/image.h"

#include <cmath>

using namespace cc::processing;

namespace {
    // Draw a filled disc of a given colour on top of a background-coloured image.
    cc::Image make_disc_image(int size, int cx, int cy, int radius,
                              uint8_t bg_b, uint8_t bg_g, uint8_t bg_r,
                              uint8_t fg_b, uint8_t fg_g, uint8_t fg_r) {
        cc::Image img(size, size, 3);

        for (int y = 0; y < size; ++y) {
            uint8_t* row = img.ptr(y);
            for (int x = 0; x < size; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                bool inside = (dx * dx + dy * dy) <= radius * radius;
                row[x * 3 + 0] = inside ? fg_b : bg_b;
                row[x * 3 + 1] = inside ? fg_g : bg_g;
                row[x * 3 + 2] = inside ? fg_r : bg_r;
            }
        }
        return img;
    }
}

TEST_CASE("edge_mask - disc on high-contrast background produces filled mask", "[edge_mask]") {
    const int size = 120;
    cc::Image source = make_disc_image(size, 60, 60, 30,
                                       255, 255, 255,
                                         0,   0,   0);
    cc::Image mask      (size, size, 1);
    cc::Image foreground(size, size, 3);

    determine_foreground_by_edges(50, source, mask, foreground);

    // The disc centre should be marked as foreground (it's inside the closed edge ring).
    REQUIRE(mask.at(60, 60)[0] == 255);

    // A point far outside the disc should be background.
    REQUIRE(mask.at(5, 5)[0] == 0);
    REQUIRE(mask.at(115, 115)[0] == 0);

    // Reasonable number of foreground pixels — should be around π·r² = ~2827,
    // plus some slop from the morphological close growing the edge ring.
    int fg_count = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (mask.at(y, x)[0]) ++fg_count;

    REQUIRE(fg_count > 2000);
    REQUIRE(fg_count < 5000);
}

TEST_CASE("edge_mask - contour trace + most_circular selector picks the disc", "[edge_mask]") {
    const int size = 160;
    cc::Image source = make_disc_image(size, 80, 80, 40,
                                       220, 220, 220,
                                        40,  60,  90);
    cc::Image mask      (size, size, 1);
    cc::Image foreground(size, size, 3);
    cc::Image output    = source.clone();

    determine_foreground_by_edges(50, source, mask, foreground);

    auto contours = find_contours(mask);
    REQUIRE(!contours.empty());

    auto result = process_contours(contours, output, e_ContourSelector::most_circular);
    REQUIRE(result.has_value());

    // Centroid should land near the true disc centre (within a few pixels).
    REQUIRE(std::abs(result->m_Centroid.x - 80) < 5);
    REQUIRE(std::abs(result->m_Centroid.y - 80) < 5);
}

TEST_CASE("edge_mask - tolerates uniform background", "[edge_mask]") {
    // A single-colour image has no edges; the mask must come back empty
    // without the flood-fill sweeping in the whole frame.
    const int size = 80;
    cc::Image source(size, size, 3);
    for (int y = 0; y < size; ++y) {
        uint8_t* row = source.ptr(y);
        for (int x = 0; x < size; ++x) {
            row[x * 3 + 0] = 128;
            row[x * 3 + 1] = 128;
            row[x * 3 + 2] = 128;
        }
    }
    cc::Image mask      (size, size, 1);
    cc::Image foreground(size, size, 3);

    determine_foreground_by_edges(50, source, mask, foreground);

    int fg_count = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (mask.at(y, x)[0]) ++fg_count;

    // Without gradients, nothing should be marked foreground.
    REQUIRE(fg_count == 0);
}
