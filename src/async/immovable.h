#ifndef ASYNC_IMMOVABLE_H
#define ASYNC_IMMOVABLE_H

namespace cc::async {
    // helper to ensure that derived classes are 'pinned' to a specific memory location
    struct Immovable {
        Immovable()            = default;
        Immovable(Immovable&&) = delete;
    };
}

#endif