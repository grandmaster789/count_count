#ifndef MATH_SQUARE_H
#define MATH_SQUARE_H

namespace cc {
    template <typename T>
    auto square(T value) {
        return value * value;
    }
}

#endif