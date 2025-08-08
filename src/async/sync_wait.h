#ifndef ASYNC_SYNC_WAIT_H
#define  ASYNC_SYNC_WAIT_H

#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <optional>

#include "result.h"

namespace cc::async {
    struct SyncWaitControlBlock {
        std::mutex              m_Mutex;
        std::condition_variable m_Condition;
        std::exception_ptr      m_Error;
        bool                    m_Completed = false;
    };

    template <typename t_Value>
    struct SyncWaitReceiver {
        SyncWaitControlBlock&   m_ControlBlock;
        std::optional<t_Value>& m_Result;

        friend void set_value(SyncWaitReceiver self, auto&& value) {
            std::unique_lock guard(self.m_ControlBlock.m_Mutex);

            self.m_Result.emplace(std::forward<decltype(value)>(value));
            self.m_ControlBlock.m_Completed = true;
            self.m_ControlBlock.m_Condition.notify_one();
        }

        friend void set_error(SyncWaitReceiver self, std::exception_ptr err) {
            std::unique_lock guard(self.m_ControlBlock.m_Mutex);

            self.m_ControlBlock.m_Error     = err;
            self.m_ControlBlock.m_Completed = true;
            self.m_ControlBlock.m_Condition.notify_one();
        }

        friend void set_stopped(SyncWaitReceiver self) {
            std::unique_lock guard(self.m_ControlBlock.m_Mutex);

            self.m_ControlBlock.m_Completed = true;
            self.m_ControlBlock.m_Condition.notify_one();
        }
    };

    template <typename t_Sender>
    std::optional<sender_result_t<t_Sender>> sync_wait(t_Sender sender) {
        using T = sender_result_t<t_Sender>;

        SyncWaitControlBlock control;
        std::optional<T>     result;

        auto operational_state = connect(sender, SyncWaitReceiver<T> { control, result });
        start(operational_state);

        // wait for the operation to complete
        std::unique_lock guard(control.m_Mutex);
        control.m_Condition.wait(guard, [&] { return control.m_Completed; });

        if (control.m_Error)
            std::rethrow_exception(control.m_Error);

        return result;
    }
}

#endif
