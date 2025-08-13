#ifndef ASYNC_COUT_RECEIVER_INL
#define ASYNC_COUT_RECEIVER_INL

#include "cout_receiver.h"

namespace cc::async {
    template <typename T>
    void CoutReceiver::set_value(T&& value) {
        std::cout << "[async]:> " << std::forward<T>(value) << '\n';
    }

    inline void CoutReceiver::set_error(std::exception_ptr ptr) {
        std::cout << "[async]:> Exception\n";
        std::rethrow_exception(ptr);
    }

    inline void CoutReceiver::set_stopped() {
        std::cout << "[async]:> Stopped\n";
    }
}

#endif
