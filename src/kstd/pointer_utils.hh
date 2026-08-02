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
