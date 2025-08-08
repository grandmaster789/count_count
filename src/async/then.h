#ifndef ASYNC_THEN_H
#define ASYNC_THEN_H

#include <stdexcept>
#include "immovable.h"
#include "result.h"

namespace cc::async {
    template <
        typename t_Receiver,
        typename t_Function
    >
    struct ThenReceiver {
        t_Receiver m_Receiver;
        t_Function m_Function;

        friend void set_value(
            ThenReceiver self,
            auto         value
        ) {
            set_value(
                self.m_Receiver,
                self.m_Function(value)
            );
        }

        friend void set_error(
            ThenReceiver       self,
            std::exception_ptr err
        ) {
            set_error(
                self.m_Receiver,
                err
            );
        }

        friend void set_stopped(
            ThenReceiver self
        ) {
            set_stopped(
                self.m_Receiver
            );
        }
    };

    template <
        typename t_Sender,
        typename t_Receiver,
        typename t_Function
    >
    struct ThenOperation:
        Immovable
    {
        connect_result_t<
            t_Sender,
            ThenReceiver<t_Receiver, t_Function>
        > m_OperationState;

        friend void start(ThenOperation& self) {
            start(self.m_OperationState);
        }
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
        friend ThenOperation<t_Sender, t_Receiver, t_Function> connect(
            ThenSender self,
            t_Receiver receiver
        ) {
            return {
                {},
                connect(
                    self.m_Sender,
                    ThenReceiver<t_Receiver, t_Function>{
                        receiver,
                        self.m_Function
                    }
                )
            };
        }
    };

    template <typename t_Sender, typename t_Function>
    ThenSender<t_Sender, t_Function> then(t_Sender sender, t_Function fn) {
        return {
            sender,
            fn
        };
    }
}

#endif