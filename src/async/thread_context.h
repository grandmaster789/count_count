#ifndef ASYNC_THREAD_CONTEXT_H
#define  ASYNC_THREAD_CONTEXT_H

#include "run_loop_context.h"
#include <thread>

namespace cc::async {
    class ThreadContext: RunLoop {
    public:
        // explicitly pull in methods from RunLoop
        using RunLoop::get_scheduler;
        using RunLoop::finish;

        void join();

    private:
        std::thread m_Thread{ [this] { run(); } };
    };
}

#endif