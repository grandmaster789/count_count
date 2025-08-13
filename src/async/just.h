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

    template <typename t_Value>
    struct JustSender {
        using result_t = t_Value;

        t_Value m_Value;

        template <typename t_Receiver>
        auto connect(t_Receiver receiver) -> JustOperation<t_Receiver, t_Value>;
    };

    template <typename t_Value>
    auto just(t_Value value);
}

#include "just.inl"

#endif