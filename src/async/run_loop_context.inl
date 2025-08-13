#ifndef ASYNC_RUN_LOOP_CONTEXT_INL
#define ASYNC_RUN_LOOP_CONTEXT_INL

#include "run_loop_context.h"

namespace cc::async {
    template <typename R>
    RunLoop::TaskOperation<R>::TaskOperation(
        R receiver,
        RunLoop& loop
    ):
        m_Receiver(receiver),
        m_Loop(loop)
    {
    }

    template <typename R>
    void RunLoop::TaskOperation<R>::execute() {
        m_Receiver.set_value(None{});
    }

    template <typename R>
    void RunLoop::TaskOperation<R>::start() {
        // insert the operation to the RunLoop queue
        m_Loop.push_back(this);
    }

    template <typename R>
    auto RunLoop::Sender::connect(R receiver) -> RunLoop::TaskOperation<R> {
        return { receiver, *m_Loop };
    }
}

#endif
