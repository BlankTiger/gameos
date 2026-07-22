#pragma once

#include <type_traits>

#include "array.hh"

template <typename Enum, typename Value>
struct Enum_Array {
    static_assert(std::is_enum_v<Enum>, "Enum_Array requires an enum type");

    static constexpr auto COUNT = static_cast<usize>(Enum::COUNT);

    Static_Array<Value, COUNT> backing_array;

    Value       &operator[](Enum index)       { return backing_array[static_cast<usize>(index)]; }
    const Value &operator[](Enum index) const { return backing_array[static_cast<usize>(index)]; }

    struct Entry {
        Enum   index;
        Value &value;
    };

    struct Iterator {
        Enum_Array *ea;
        usize      i;

        Entry operator*()  const { return Entry{ static_cast<Enum>(i), ea->backing_array[i] }; }
        Iterator &operator++()   { ++i; return *this; }
        bool operator!=(const Iterator &other) const { return i != other.i; }
    };

    Iterator begin() { return Iterator{ this, 0 }; }
    Iterator end()   { return Iterator{ this, COUNT }; }
};
