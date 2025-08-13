#ifndef ASYNC_COUT_RECEIVER_H
#define ASYNC_COUT_RECEIVER_H

#include <stdexcept>
#include <iostream>

namespace cc::async {
    // mostly for debugging
    struct CoutReceiver {
               template <typename T>
               void set_value(T&& value);
        inline void set_error(std::exception_ptr ptr);
        inline void set_stopped();
    };
}

#include "cout_receiver.inl"

#endif