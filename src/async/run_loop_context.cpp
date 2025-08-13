#include "run_loop_context.h"

namespace cc::async {
    void RunLoop::push_back(Task* task) {
        std::unique_lock guard(m_Mutex);

        task->m_Next   = &m_Head;
        m_Tail->m_Next = task;
        m_Tail         = task;

        m_Condition.notify_one();
    }

    RunLoop::Task* RunLoop::pop_front() {
        std::unique_lock guard(m_Mutex);

        m_Condition.wait(guard, [this] { return
                m_Finishing ||
                (m_Head.m_Next != &m_Head);
        });

        // see if the queue is empty
        if (m_Head.m_Next == &m_Head)
            return nullptr;

        Task* result = std::exchange(
                m_Head.m_Next,
                m_Head.m_Next->m_Next
        );

        // if the tail was removed by the pop operation, update it to point to the head
        if (result == m_Tail)
            m_Tail = &m_Head;

        return result;
    }

    RunLoop::Sender RunLoop::Scheduler::schedule() {
        return { m_Loop };
    }

    RunLoop::Scheduler RunLoop::get_scheduler() {
        return { this };
    }

    void RunLoop::run() {
        while (auto* work = pop_front())
            work->execute();
    }

    void RunLoop::finish() {
        std::unique_lock guard(m_Mutex);

        m_Finishing = true;
        m_Condition.notify_all();
    }
}