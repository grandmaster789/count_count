#ifndef ASYNC_JUST_H
#define ASYNC_JUST_H

#include "immovable.h"

namespace cc::async {
    template <
        typename t_Receiver,
        typename t_Value
    >
    struct JustOperation:
        Immovable
    {
        t_Receiver m_Receiver;
        t_Value    m_Value;

        friend void start(JustOperation& self) {
            set_value(
                self.m_Receiver,
                self.m_Value
            );
        }
    };

    template <typename t_Value>
    struct JustSender {
        using result_t = t_Value;

        t_Value m_Value;

        template <typename t_Receiver>
        friend JustOperation<t_Receiver, t_Value> connect(
            JustSender self,
            t_Receiver receiver
        ) {
            return {
                {},
                receiver,
                self.m_Value
            };
        }
    };

    template <typename t_Value>
    JustSender<t_Value> just(t_Value value) {
        return {
            value
        };
    }
}

#endif