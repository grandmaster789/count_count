#ifndef CC_TYPES_COLOR_H
#define CC_TYPES_COLOR_H

#include <cstdint>

namespace cc {
    struct Color3 {
        double b {};
        double g {};
        double r {};

        Color3() = default;
        Color3(double b, double g, double r): b(b), g(g), r(r) {}

        double  operator [] (int i) const { return i == 0 ? b : (i == 1 ? g : r); }
        double& operator [] (int i)       { return i == 0 ? b : (i == 1 ? g : r); }

        bool operator == (const Color3& rhs) const = default;
        bool operator != (const Color3& rhs) const = default;
    };
}

#endif
