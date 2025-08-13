#ifndef ASYNC_THEN_H
#define ASYNC_THEN_H

#include <stdexcept>
#include "result.h"

namespace cc::async {
    template <
        typename t_Receiver,
        typename t_Function
    >
    struct ThenReceiver {
        t_Receiver m_Receiver;
        t_Function m_Function;

        void set_value(auto value);
        void set_error(std::exception_ptr err);
        void set_stopped();
    };

    template <
        typename t_Sender,
        typename t_Receiver,
        typename t_Function
    >
    struct ThenOperation {
        connect_result_t<
            t_Sender,
            ThenReceiver<t_Receiver, t_Function>
        > m_OperationState;

        void start();
    };

    template <
        typename t_Sender,
        typename t_Function
    >
    struct ThenSender {
        using result_t = std::invoke_result_t<
            t_Function,
            sender_result_t<t_Sender>
        >;

        t_Sender   m_Sender;
        t_Function m_Function;

        template <typename t_Receiver>
        auto connect(t_Receiver receiver) -> ThenOperation<t_Sender, t_Receiver, t_Function>;
    };

    template <typename t_Sender, typename t_Function>
    auto then(t_Sender sender, t_Function fn);
}

#include "then.inl"

#endif