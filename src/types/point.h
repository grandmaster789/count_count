#ifndef CC_TYPES_POINT_H
#define CC_TYPES_POINT_H

#include <cmath>

namespace cc {
    template <typename T>
    struct Point2 {
        T x {};
        T y {};

        Point2() = default;
        Point2(T x, T y): x(x), y(y) {}

        template <typename U>
        explicit Point2(const Point2<U>& other):
            x(static_cast<T>(other.x)),
            y(static_cast<T>(other.y))
        {}

        Point2 operator + (const Point2& rhs) const { return { static_cast<T>(x + rhs.x), static_cast<T>(y + rhs.y) }; }
        Point2 operator - (const Point2& rhs) const { return { static_cast<T>(x - rhs.x), static_cast<T>(y - rhs.y) }; }
        Point2 operator * (T scalar)          const { return { static_cast<T>(x * scalar), static_cast<T>(y * scalar) }; }

        Point2& operator += (const Point2& rhs) { x += rhs.x; y += rhs.y; return *this; }
        Point2& operator -= (const Point2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }

        bool operator == (const Point2& rhs) const = default;
        bool operator != (const Point2& rhs) const = default;
    };

    using Point2i = Point2<int>;
    using Point2f = Point2<float>;
    using Point2d = Point2<double>;
}

#endif
