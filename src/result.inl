#ifndef COUNTVONCOUNT_RESULT_INL
#define COUNTVONCOUNT_RESULT_INL

#include "result.h"

namespace cvc {
    template <typename T>
    constexpr Result<T>::Result(T&& value) noexcept:
        m_Value(std::forward<T>(value))
    {
    }

    template <typename T>
    constexpr Result<T>::Result(const T& value):
        m_Value(value)
    {
    }

    template <typename T>
    Result<T>::Result(std::error_code err) noexcept:
        m_Value(err)
    {
    }

    template <typename T>
    constexpr Result<T>::Result(const Result& r):
        m_Value(r.m_Value)
    {
    }

    template <typename T>
    constexpr Result<T>& Result<T>::operator = (const Result& r) {
        m_Value = r.m_Value;
        return *this;
    }

    template <typename T>
    constexpr Result<T>::Result(Result&& r) noexcept:
        m_Value(std::move(r.m_Value))
    {
    }

    template <typename T>
    constexpr Result<T>& Result<T>::operator = (Result&& r) noexcept {
        std::swap(m_Value, r.m_Value);
        return *this;
    }

    template <typename T>
    constexpr Result<T>::operator bool() const noexcept {
        return (m_Value.index() == 0);
    }

    template <typename T>
    constexpr T& Result<T>::operator *() {
        if (m_Value.index())
            throw BadResult();

        return std::get<0>(m_Value);
    }

    template <typename T>
    constexpr const T& Result<T>::operator *() const {
        if (m_Value.index())
            throw BadResult();

        return std::get<0>(m_Value);
    }

    template <typename T>
    constexpr T* Result<T>::operator ->() {
        if (m_Value.index())
            throw BadResult();

        return &std::get<0>(m_Value);
    }

    template <typename T>
    constexpr const T* Result<T>::operator ->() const {
        if (m_Value.index())
            throw BadResult();

        return &std::get<0>(m_Value);
    }

    template <typename T>
    [[nodiscard]]
    std::error_code Result<T>::error() const noexcept {
        if (m_Value.index())
            return std::get<1>(m_Value);
        return {};
    }
}

#endif
