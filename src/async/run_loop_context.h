#ifndef ASYNC_RUN_LOOP_CONTEXT_H
#define ASYNC_RUN_LOOP_CONTEXT_H

#include <mutex>
#include <condition_variable>

namespace cc::async {
    /*
     * Use singly linked list to create a loop of tasks
     * These are defined during compilation and do not allocate memory during runtime
     */
    struct RunLoop {
        struct None {};

        struct Task {
            Task* m_Next = this;

            virtual void execute() {} // maybe operator() would be nicer
        };

        template <typename t_Receiver>
        struct TaskOperation: Task {
            t_Receiver m_Receiver;
            RunLoop&   m_Loop;

            TaskOperation(
                t_Receiver receiver,
                RunLoop&   loop
            );

            void execute() final;
            void start();
        };

        void push_back(Task* task);
        Task* pop_front();

        struct Sender {
            using result_t = None;

            RunLoop* m_Loop;

            template <typename t_Receiver>
            auto connect(t_Receiver receiver) -> TaskOperation<t_Receiver>;
        };

        struct Scheduler {
            RunLoop* m_Loop;

            Sender schedule();
        };

        Scheduler get_scheduler();
        void      run();
        void      finish();

        Task                    m_Head;
        Task*                   m_Tail = &m_Head;
        std::mutex              m_Mutex;
        std::condition_variable m_Condition;
        bool                    m_Finishing = false;
    };
}

#include "run_loop_context.inl"

#endif