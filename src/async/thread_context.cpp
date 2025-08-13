#include "thread_context.h"

namespace cc::async {
    void ThreadContext::join() {
        m_Thread.join();
    }
}