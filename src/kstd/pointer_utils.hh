#pragma once

#include "basic.hh"

template <typename T>
force_inline auto ptr_addr(const T* pointer) -> psize {
    return reinterpret_cast<psize>(pointer);
}

// Add a byte offset to a pointer. Keeps the pointed-to type (and cv).
template <typename T>
force_inline auto ptr_offset(T* pointer, psize byte_offset) -> T* {
    return reinterpret_cast<T*>(reinterpret_cast<psize>(pointer) + byte_offset);
}

// Cast a raw address (e.g. physical under identity map) to a typed pointer.
// psize intermediate avoids -Warray-bounds on fixed low addresses.
template <typename T>
force_inline auto addr_as(u64 address) -> T* {
    return reinterpret_cast<T*>(static_cast<psize>(address));
}
