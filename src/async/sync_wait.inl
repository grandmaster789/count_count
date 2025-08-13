#ifndef ASYNC_SYNC_WAIT_INL
#define ASYNC_SYNC_WAIT_INL

#include "sync_wait.h"

namespace cc::async {
    template <typename V>
    template <typename U>
    void SyncWaitReceiver<V>::set_value(U &&value) {
        std::unique_lock guard(m_ControlBlock.m_Mutex);

        m_Result.emplace(std::forward<U>(value));
        m_ControlBlock.m_Completed = true;
        m_ControlBlock.m_Condition.notify_one();
    }

    template <typename V>
    void SyncWaitReceiver<V>::set_error(std::exception_ptr err) {
        std::unique_lock guard(m_ControlBlock.m_Mutex);

        m_ControlBlock.m_Error     = err;
        m_ControlBlock.m_Completed = true;
        m_ControlBlock.m_Condition.notify_one();
    }

    template <typename V>
    void SyncWaitReceiver<V>::set_stopped() {
        std::unique_lock guard(m_ControlBlock.m_Mutex);

        m_ControlBlock.m_Completed = true;
        m_ControlBlock.m_Condition.notify_one();
    }

    template <typename S>
    auto sync_wait(S sender) {
        using T = sender_result_t<S>;

        SyncWaitControlBlock control;
        std::optional<T>     result;

        auto operational_state = sender.connect(SyncWaitReceiver<T> { control, result });
        operational_state.start();

        // wait for the operation to complete
        std::unique_lock guard(control.m_Mutex);
        control.m_Condition.wait(guard, [&] { return control.m_Completed; });

        if (control.m_Error)
            std::rethrow_exception(control.m_Error);

        return result;
    }
}

#endif
