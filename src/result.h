#ifndef COUNTVONCOUNT_RESULT_H
#define COUNTVONCOUNT_RESULT_H

#include <stdexcept>
#include <iostream>
#include <variant>

namespace cvc {
    class BadResult:
        public std::logic_error
    {
    public:
        BadResult();
        explicit BadResult(const char* what);
    };

    template <typename T>
    class [[nodiscard]] Result {
    public:
        // allow implicit conversion from any type, as well as error_code
        constexpr Result(T&& value) noexcept;
        constexpr Result(const T& value);
                  Result(std::error_code err) noexcept;
        constexpr ~Result() = default;

        constexpr Result             (const Result& r);
        constexpr Result& operator = (const Result& r);
        constexpr Result             (Result&& r) noexcept;
        constexpr Result& operator = (Result&& r) noexcept;

        explicit constexpr operator bool() const noexcept;

        constexpr       T& operator *();
        constexpr const T& operator *() const;
        constexpr       T* operator ->();
        constexpr const T* operator ->() const;

        [[nodiscard]] std::error_code error() const noexcept;

    private:
        std::variant<T, std::error_code> m_Value;
    };
}

#include "result.inl"
#include "result_void.inl"

#endif
