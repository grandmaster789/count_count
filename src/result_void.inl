#ifndef COUNTVONCOUNT_RESULT_VOID_INL
#define COUNTVONCOUNT_RESULT_VOID_INL

#include "result.h"
#include <optional>

namespace cvc {
    template <>
    class [[nodiscard]] Result<void> {
    public:
        // allow implicit conversion from any type, as well as error_code
        constexpr Result() noexcept = default;

        Result(std::error_code err) noexcept:
            m_Error(err)
        {
        }

        explicit constexpr operator bool() const noexcept {
            return !m_Error.has_value();
        }

        [[nodiscard]] std::error_code error() const noexcept {
            if (m_Error.has_value())
                return *m_Error;

            return{};
        }

    private:
        std::optional<std::error_code> m_Error;
    };
}

#endif