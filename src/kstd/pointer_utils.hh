#pragma once

#include "basic.hh"

template <typename T>
force_inline auto ptr_addr(const T* pointer) -> psize {
    return reinterpret_cast<psize>(pointer);
}
