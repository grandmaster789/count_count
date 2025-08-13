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

        template <typename U>
        void set_value(U&& value);
        void set_error(std::exception_ptr err);
        void set_stopped();
    };

    template <typename t_Sender>
    auto sync_wait(t_Sender sender);
}

#include "sync_wait.inl"

#endif
