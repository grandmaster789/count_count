#ifndef ASYNC_THEN_INL
#define ASYNC_THEN_INL

#include "then.h"

namespace cc::async {
        template <typename R, typename F>
        void ThenReceiver<R, F>::set_value(auto value) {
            m_Receiver.set_value(
                m_Function(value)
            );
        }

        template <typename R, typename F>
        void ThenReceiver<R, F>::set_error(std::exception_ptr err) {
            m_Receiver.set_error(err);
        }

        template <typename R, typename F>
        void ThenReceiver<R, F>::set_stopped() {
            m_Receiver.set_stopped();
        }

        template <typename S, typename R, typename F>
        void ThenOperation<S, R, F>::start() {
            m_OperationState.start();
        }

        template <typename S, typename F>
        template <typename R>
        ThenOperation<S, R, F> ThenSender<S, F>::connect(R receiver) {
            return {
                m_Sender.connect(
                    ThenReceiver<R, F>{
                        receiver,
                        m_Function
                    }
                )
            };
        }

    template <typename S, typename F>
    auto then(S sender, F fn) {
        return ThenSender<S, F>{
            sender,
            fn
        };
    }
}

#endif
