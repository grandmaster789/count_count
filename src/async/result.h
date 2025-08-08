#ifndef ASYNC_RESULT_H
#define ASYNC_RESULT_H

#include <utility>

namespace cc::async {
    template <
        typename t_Sender,
        typename t_Receiver
    >
    using connect_result_t = decltype(
        connect(
            std::declval<t_Sender>(),
            std::declval<t_Receiver>()
        )
    );

    template <typename t_Sender>
    using sender_result_t = typename t_Sender::result_t;
}

#endif