#ifndef UTIL_FLAT_MAP_INL
#define UTIL_FLAT_MAP_INL

#include "flat_map.h"
#include <stdexcept>

namespace cc {
    template <typename K, typename V>
    void FlatMap<K, V>::insert(const K& key, V&& value) {
        auto it = std::find(
            std::begin(m_Keys),
            std::end(m_Keys),
            key
        );

        if (it == std::end(m_Keys)) {
            // create a new entry
            m_Keys.push_back(key);
            m_Values.push_back(std::forward<V>(value));
        }
        else {
            // overwrite the current entry
            auto idx = std::distance(
                std::begin(m_Keys),
                it
            );

            m_Values[idx] = std::forward<V>(value);
        }
    }

    template <typename K, typename V>
    void FlatMap<K, V>::remove(const K& key) {
        auto it = std::find(
            std::begin(m_Keys),
            std::end(m_Keys),
            key
        );

        if (it == std::end(m_Keys))
            return; // silently ignore key not found

        size_t idx = std::distance(
            std::begin(m_Keys),
            it
        );

        m_Values.erase(
            std::next(
                std::begin(m_Values),
                idx
            )
        );
        m_Keys.erase(it);
    }

    template <typename K, typename V>
    bool FlatMap<K, V>::contains(const K& key) const {
        return std::find(
            std::begin(m_Keys),
            std::end(m_Keys),
            key
        ) != std::end(m_Keys);
    }

    template <typename K, typename V>
    size_t FlatMap<K, V>::get_num_entries() const {
        return m_Keys.size();
    }

    template <typename K, typename V>
    V& FlatMap<K, V>::operator[](const K& key) {
        auto it = std::find(
            std::begin(m_Keys),
            std::end(m_Keys),
            key
        );

        if (it == std::end(m_Keys))
            throw std::runtime_error("Key not found");

        auto idx = std::distance(
            std::begin(m_Keys),
            it
        );

        return m_Values[idx];
    }

    template <typename K, typename V>
    const V& FlatMap<K, V>::operator[](const K& key) const {
        auto it = std::find(
            std::cbegin(m_Keys),
            std::cend(m_Keys),
            key
        );

        if (it == std::cend(m_Keys))
            throw std::runtime_error("Key not found");

        auto idx = std::distance(
            std::cbegin(m_Keys),
            it
        );

        return m_Values[idx];
    }

    template <typename K, typename V>
    void FlatMap<K, V>::clear() {
        m_Keys.clear();
        m_Values.clear();
    }
}

#endif