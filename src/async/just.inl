#ifndef ASYNC_JUST_INL
#define ASYNC_JUST_INL

#include "just.h"

namespace cc::async {
    template <typename R, typename T>
    void JustOperation<R, T>::start() {
        m_Receiver.set_value(m_Value);
    }

    template <typename T>
    template <typename R>
    auto JustSender<T>::connect(R receiver) -> JustOperation<R, T> {
        return {
            receiver,
            m_Value
        };
    }

    template <typename T>
    auto just(T value) {
        return JustSender<T>{
            value
        };
    }
}

#endif
