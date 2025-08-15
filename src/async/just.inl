#ifndef ASYNC_JUST_INL
#define ASYNC_JUST_INL

#include "just.h"

namespace cc::async {
    template <typename R, typename T>
    void JustOperation<R, T>::start() {
        m_Receiver.set_value(m_Value);
    }

    template <typename R>
    void JustErrorOperation<R>::start() {
        m_Receiver.set_error(m_Error);
    }

    template <typename R>
    void JustStoppedOperation<R>::start() {
        m_Receiver.set_stopped();
    }

    template <typename T>
    template <typename R>
    auto JustSender<T>::connect(R receiver) -> JustOperation<R, T> {
        return {
            receiver,
            m_Value
        };
    }

    template <typename R>
    auto JustErrorSender::connect(R receiver) -> JustErrorOperation<R> {
        return {
            receiver,
            m_Error
        };
    }

    template <typename R>
    auto JustStoppedSender::connect(R receiver) -> JustStoppedOperation<R> {
        return {
            receiver
        };
    }

    template <typename T>
    auto just(T value) -> JustSender<T> {
        return JustSender<T>{
            value
        };
    }

    auto just_error(std::exception_ptr error) -> JustErrorSender {
        return JustErrorSender {
            error
        };
    }

    auto just_stopped() -> JustStoppedSender {
        return JustStoppedSender {};
    }
}

#endif
