#include "drawing.h"

#include <cmath>
#include <algorithm>

namespace cc::drawing {

    void set_pixel(
        Image& img,
        const int x,
        const int y,
        const uint8_t b,
        const uint8_t g,
        const uint8_t r
    ) {
        if (x < 0 || x >= img.cols() || y < 0 || y >= img.rows())
            return;

        if (img.channels() == 3) {
            uint8_t* p = img.at(y, x);
            p[0] = b;
            p[1] = g;
            p[2] = r;
        }
        else if (img.channels() == 1) {
            img.at(y, x)[0] = b; // use first component for grayscale
        }
    }

    // Set a thick pixel (filled square of given thickness centered on x,y)
    static void set_thick_pixel(
        Image& img,
        const int x,
        const int y,
        const uint8_t b,
        const uint8_t g,
        const uint8_t r,
        const int thickness
    ) {
        const int half = thickness / 2;

        for (int dy = -half; dy <= half; ++dy)
            for (int dx = -half; dx <= half; ++dx)
                set_pixel(img, x + dx, y + dy, b, g, r);
    }

    void draw_line(
        Image&         img,
        const Point2i& p1,
        const Point2i& p2,
        const uint8_t b,
        const uint8_t g,
        const uint8_t r,
        const int thickness
    ) {
        int x0 = p1.x, y0 = p1.y;
        const int x1 = p2.x;
        const int y1 = p2.y;

        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            if (thickness <= 1)
                set_pixel(img, x0, y0, b, g, r);
            else
                set_thick_pixel(img, x0, y0, b, g, r, thickness);

            if (x0 == x1 && y0 == y1)
                break;

            const int e2 = 2 * err;

            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }

            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void draw_arrowed_line(
        Image&         img,
        const Point2i& from,
        const Point2i& to,
        const uint8_t b,
        const uint8_t g,
        const uint8_t r,
        const int thickness
    ) {
        draw_line(img, from, to, b, g, r, thickness);

        // Draw arrowhead
        const double angle = std::atan2(
            static_cast<double>(to.y - from.y),
            static_cast<double>(to.x - from.x)
        );
        const double arrow_len = 10.0 * thickness;
        constexpr double arrow_angle = 0.5; // radians (~28 degrees)

        Point2i tip1(
            static_cast<int>(to.x - arrow_len * std::cos(angle - arrow_angle)),
            static_cast<int>(to.y - arrow_len * std::sin(angle - arrow_angle))
        );
        Point2i tip2(
            static_cast<int>(to.x - arrow_len * std::cos(angle + arrow_angle)),
            static_cast<int>(to.y - arrow_len * std::sin(angle + arrow_angle))
        );

        draw_line(img, to, tip1, b, g, r, thickness);
        draw_line(img, to, tip2, b, g, r, thickness);
    }

    void draw_circle(
        Image&         img,
        const Point2i& center,
        const int      radius,
        const uint8_t  b,
        const uint8_t  g,
        const uint8_t  r,
        const bool     filled
    ) {
        if (filled) {
            for (int y = -radius; y <= radius; ++y) {
                for (int x = -radius; x <= radius; ++x) {
                    if (x * x + y * y <= radius * radius) {
                        set_pixel(img, center.x + x, center.y + y, b, g, r);
                    }
                }
            }
            return;
        }

        // Midpoint circle algorithm
        int x = radius;
        int y = 0;
        int err = 1 - radius;

        while (x >= y) {
            set_pixel(img, center.x + x, center.y + y, b, g, r);
            set_pixel(img, center.x + y, center.y + x, b, g, r);
            set_pixel(img, center.x - y, center.y + x, b, g, r);
            set_pixel(img, center.x - x, center.y + y, b, g, r);
            set_pixel(img, center.x - x, center.y - y, b, g, r);
            set_pixel(img, center.x - y, center.y - x, b, g, r);
            set_pixel(img, center.x + y, center.y - x, b, g, r);
            set_pixel(img, center.x + x, center.y - y, b, g, r);

            ++y;
            if (err < 0) {
                err += 2 * y + 1;
            } else {
                --x;
                err += 2 * (y - x) + 1;
            }
        }
    }

    void draw_polyline(
        Image&                          img,
        const std::vector<Point2i>&     points,
        const uint8_t b,
        const uint8_t g,
        const uint8_t r,
        const bool closed,
        const int  thickness
    ) {
        if (points.size() < 2)
            return;

        for (size_t i = 0; i + 1 < points.size(); ++i)
            draw_line(img, points[i], points[i + 1], b, g, r, thickness);

        if (closed && points.size() > 2)
            draw_line(img, points.back(), points.front(), b, g, r, thickness);
    }

    // ---- Embedded bitmap font for digits + a few symbols ----
    // Each glyph is 5 wide x 7 tall, stored as 7 bytes (each byte = 5 bits, MSB = left)
    // Characters: '0'-'9', '/', '?', '!', ' '

    static constexpr int k_GlyphW = 5;
    static constexpr int k_GlyphH = 7;

    // clang-format off
    static constexpr uint8_t k_Font[][k_GlyphH] = {
        // '0'
        {   0b01110,
            0b10001,
            0b10011,
            0b10101,
            0b11001,
            0b10001,
            0b01110 },

        // '1'
        {   0b00100,
            0b01100,
            0b00100,
            0b00100,
            0b00100,
            0b00100,
            0b01110 },

        // '2'
        {   0b01110,
            0b10001,
            0b00001,
            0b00010,
            0b00100,
            0b01000,
            0b11111 },

        // '3'
        {   0b11111,
            0b00010,
            0b00100,
            0b00010,
            0b00001,
            0b10001,
            0b01110 },

        // '4'
        {   0b00010,
            0b00110,
            0b01010,
            0b10010,
            0b11111,
            0b00010,
            0b00010 },

        // '5'
        {   0b11111,
            0b10000,
            0b11110,
            0b00001,
            0b00001,
            0b10001,
            0b01110 },

        // '6'
        {   0b00110,
            0b01000,
            0b10000,
            0b11110,
            0b10001,
            0b10001,
            0b01110 },

        // '7'
        {   0b11111,
            0b00001,
            0b00010,
            0b00100,
            0b01000,
            0b01000,
            0b01000 },

        // '8'
        {   0b01110,
            0b10001,
            0b10001,
            0b01110,
            0b10001,
            0b10001,
            0b01110 },

        // '9'
        {   0b01110,
            0b10001,
            0b10001,
            0b01111,
            0b00001,
            0b00010,
            0b01100 },

        // '/'
        {   0b00001,
            0b00010,
            0b00010,
            0b00100,
            0b01000,
            0b01000,
            0b10000 },

        // '?'
        {   0b01110,
            0b10001,
            0b00001,
            0b00010,
            0b00100,
            0b00000,
            0b00100 },

        // '!'
        {   0b00100,
            0b00100,
            0b00100,
            0b00100,
            0b00100,
            0b00000,
            0b00100 },

        // ' '
        {   0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000,
            0b00000 },

        // '('
        {   0b00010,
            0b00100,
            0b01000,
            0b01000,
            0b01000,
            0b00100,
            0b00010 },

        // ')'
        {   0b01000,
            0b00100,
            0b00010,
            0b00010,
            0b00010,
            0b00100,
            0b01000 },

        // '%'
        {   0b11000,
            0b11001,
            0b00010,
            0b00100,
            0b01000,
            0b10011,
            0b00011 },
    };
    // clang-format on

    static int glyph_index(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c == '/') return 10;
        if (c == '?') return 11;
        if (c == '!') return 12;
        if (c == '(') return 14;
        if (c == ')') return 15;
        if (c == '%') return 16;
        return 13; // space / unknown
    }

    TextSize measure_text(const std::string& text, double scale) {
        const int s = std::max(1, static_cast<int>(scale));
        const int char_w = (k_GlyphW + 1) * s; // +1 for inter-character spacing

        return {
            static_cast<int>(text.size()) * char_w,
            k_GlyphH * s
        };
    }

    void draw_text(
        Image&         img,
        const std::string& text,
        const Point2i& position,
        const uint8_t b,
        const uint8_t g,
        const uint8_t r,
        const double scale
    ) {
        int s = std::max(1, static_cast<int>(scale));
        int char_w = (k_GlyphW + 1) * s;

        for (size_t ci = 0; ci < text.size(); ++ci) {
            int gi = glyph_index(text[ci]);
            int base_x = position.x + static_cast<int>(ci) * char_w;
            int base_y = position.y;

            for (int gy = 0; gy < k_GlyphH; ++gy) {
                uint8_t row_bits = k_Font[gi][gy];
                for (int gx = 0; gx < k_GlyphW; ++gx) {
                    if (row_bits & (1 << (k_GlyphW - 1 - gx))) {
                        // scale up
                        for (int sy = 0; sy < s; ++sy)
                            for (int sx = 0; sx < s; ++sx)
                                set_pixel(img, base_x + gx * s + sx, base_y + gy * s + sy, b, g, r);
                    }
                }
            }
        }
    }
}
