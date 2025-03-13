#ifndef COUNTVONCOUNT_THREADSAFE_QUEUE_H
#define COUNTVONCOUNT_THREADSAFE_QUEUE_H

#include <optional>
#include <mutex>
#include <queue>

namespace cvc {
    // NOTE this is sort of mixed in the interface; blocking pushes with dynamic allocations and non-blocking pops
    //      very straightforward implementation though
    template <typename T>
    class ThreadsafeQueue {
    public:
        template <typename U> // NOTE U must be convertible to T
        void push(U&& value) {
            std::lock_guard guard(m_Mutex);
            m_Queue.push(std::forward<U>(value));
        }

        std::optional<T> pop() {
            std::lock_guard guard(m_Mutex);
            if (m_Queue.empty())
                return std::nullopt;

            std::optional<T> result(std::move(m_Queue.front()));
            m_Queue.pop();

            return result;
        }

    private:
        std::mutex    m_Mutex;
        std::queue<T> m_Queue;
    };
}

#endif //COUNTVONCOUNT_THREADSAFE_QUEUE_H
