#ifndef ASYNC_JUST_H
#define ASYNC_JUST_H

namespace cc::async {
    template <
        typename t_Receiver,
        typename t_Value
    >
    struct JustOperation {
        t_Receiver m_Receiver;
        t_Value    m_Value;

        void start();
    };

    template <typename t_Receiver>
    struct JustErrorOperation {
        t_Receiver         m_Receiver;
        std::exception_ptr m_Error;

        void start();
    };

    template <typename t_Receiver>
    struct JustStoppedOperation {
        t_Receiver m_Receiver;

        void start();
    };

    template <typename t_Value>
    struct JustSender {
        using result_t = t_Value;

        t_Value m_Value;

        template <typename t_Receiver>
        auto connect(t_Receiver receiver) -> JustOperation<t_Receiver, t_Value>;
    };

    struct JustErrorSender {
        std::exception_ptr m_Error;

        template <typename t_Receiver>
        auto connect(t_Receiver receiver) -> JustErrorOperation<t_Receiver>;
    };

    struct JustStoppedSender {
        template <typename t_Receiver>
        auto connect(t_Receiver receiver) -> JustStoppedOperation<t_Receiver>;
    };

    template <typename t_Value>
    auto just        (t_Value value)            -> JustSender<t_Value>;
    auto just_error  (std::exception_ptr error) -> JustErrorSender;
    auto just_stopped()                         -> JustStoppedSender;
}

#include "just.inl"

#endif