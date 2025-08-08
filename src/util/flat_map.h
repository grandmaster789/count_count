#ifndef UTIL_FLAT_MAP_H
#define UTIL_FLAT_MAP_H

#include <vector>

namespace cc {
    //
    // straightforward, no-frills implementation of a flat map
    //
    template <
        typename t_Key,
        typename t_Value
    >
    class FlatMap {
    public:
        void insert(const t_Key& key, t_Value&& value); // will overwrite if key is already present
        void remove(const t_Key& key);                  // will ignore keys that are not found

        [[nodiscard]]
        bool contains(const t_Key& key) const;

        [[nodiscard]]
        size_t get_num_entries() const;

              t_Value& operator[](const t_Key& key);       // will throw when key is not found
        const t_Value& operator[](const t_Key& key) const; // will throw when key is not found

        void clear();

    private:
        std::vector<t_Key>   m_Keys;
        std::vector<t_Value> m_Values;
    };
}

#include "flat_map.inl"

#endif
