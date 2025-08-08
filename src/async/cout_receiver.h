#ifndef ASYNC_COUT_RECEIVER_H
#define ASYNC_COUT_RECEIVER_H

#include <stdexcept>
#include <iostream>

namespace cc::async {
    // mostly for debugging
    struct CoutReceiver {
        friend void set_value(CoutReceiver, auto value) {
            std::cout << "[async]:> " << value << '\n';
        }

        friend void set_error(CoutReceiver, std::exception_ptr) {
            std::terminate();
        }

        friend void set_stopped(CoutReceiver) {
            std::terminate();
        }
    };
}

#endif