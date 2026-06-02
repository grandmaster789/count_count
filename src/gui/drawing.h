#ifndef CC_GUI_DRAWING_H
#define CC_GUI_DRAWING_H

#include "types/image.h"
#include "types/point.h"
#include "types/color.h"

#include <string>
#include <cstdint>

namespace cc::drawing {
    // Set a single pixel (bounds-checked)
    void set_pixel(
        Image& img,
        int x,
        int y,
        uint8_t b,
        uint8_t g,
        uint8_t r
    );

    // Bresenham's line algorithm
    void draw_line(
        Image&         img,
        const Point2i& p1,
        const Point2i& p2,
        uint8_t b,
        uint8_t g,
        uint8_t r,
        int thickness = 1
    );

    // Arrowed line (line + arrowhead)
    void draw_arrowed_line(
        Image&         img,
        const Point2i& from,
        const Point2i& to,
        uint8_t b,
        uint8_t g,
        uint8_t r,
        int thickness = 1
    );

    // Midpoint circle algorithm
    void draw_circle(
        Image&         img,
        const Point2i& center,
        int            radius,
        uint8_t b,
        uint8_t g,
        uint8_t r,
        bool filled = false
    );

    // Draw polyline (sequence of connected points)
    void draw_polyline(
        Image&                          img,
        const std::vector<Point2i>&     points,
        uint8_t b,
        uint8_t g,
        uint8_t r,
        bool closed     = false,
        int  thickness  = 1
    );

    // Bitmap font for digits, '/', '?', '!'
    struct TextSize {
        int width;
        int height;
    };

    TextSize measure_text(const std::string& text, double scale = 1.0);

    void draw_text(
        Image&             img,
        const std::string& text,
        const Point2i&     position,
        uint8_t b,
        uint8_t g,
        uint8_t r,
        double scale     = 1.0
    );
}

#endif
