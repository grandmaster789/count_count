#ifndef ASYNC_RUN_LOOP_CONTEXT_H
#define ASYNC_RUN_LOOP_CONTEXT_H

#include "immovable.h"
#include <mutex>
#include <condition_variable>

namespace cc::async {
    /*
     * Use singly linked list to create a loop of tasks
     * These are defined during compilation and do not allocate memory during runtime
     */
    struct RunLoop:
        Immovable
    {
        struct None {};

        struct Task: Immovable {
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
            ):
                m_Receiver(receiver),
                m_Loop(loop)
            {
            }

            void execute() override final {
                set_value(m_Receiver, None{});
            }

            friend void start(TaskOperation& self) {
                // insert the operation to the RunLoop queue

                self.m_Loop.push_back(&self);
            }
        };

        void push_back(Task* task) {
            std::unique_lock guard(m_Mutex);

            task->m_Next   = &m_Head;
            m_Tail->m_Next = task;
            m_Tail         = task;

            m_Condition.notify_one();
        }

        Task* pop_front() {
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

        struct Sender {
            using result_t = None;

            RunLoop* m_Loop;

            template <typename t_Receiver>
            friend TaskOperation<t_Receiver> connect(Sender self, t_Receiver receiver) {
                return { receiver, *self.m_Loop };
            }
        };

        struct Scheduler {
            RunLoop* m_Loop;

            friend Sender schedule(Scheduler self) {
                return { self.m_Loop };
            }
        };

        Scheduler get_scheduler() {
            return { this };
        }

        void run() {
            while (auto* work = pop_front())
                work->execute();
        }

        void finish() {
            std::unique_lock guard(m_Mutex);

            m_Finishing = true;
            m_Condition.notify_all();
        }

        Task                    m_Head;
        Task*                   m_Tail = &m_Head;
        std::mutex              m_Mutex;
        std::condition_variable m_Condition;
        bool                    m_Finishing = false;
    };
}

#endif