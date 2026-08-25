#pragma once

#include <type_traits>

#include "basic.hh"

template <typename T>
force_inline auto ptr_addr(T* pointer) -> psize {
    return cast(psize)cast(const void*)(pointer);
}

// Add a byte offset to a pointer. Keeps the pointed-to type (and cv).
template <typename T>
force_inline auto ptr_offset(T* pointer, psize byte_offset) -> T* {
    return cast(T*)(cast(psize)pointer + byte_offset);
}

template <typename T>
concept Pointer = std::is_pointer_v<T>;

// Cast a raw address (e.g. physical under identity map) to a typed pointer.
// psize intermediate avoids -Warray-bounds on fixed low addresses.
template <Pointer T>
force_inline auto addr_as(u64 address) -> T {
    return cast(T)cast(psize)(address);
}
