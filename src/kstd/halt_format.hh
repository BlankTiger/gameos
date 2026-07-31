#pragma once

#include "assert.hh"
#include "format.hh"

#if !HOSTED

// halt::print needs fmt; assert.hh cannot include this header (cycle).
namespace halt {

auto print(string format) -> int {
    return fmt::print(hidden::halt_backend, format);
}

template <typename T, typename... Rest>
auto print(string format, T&& value, Rest&&... rest) -> int {
    return fmt::print(hidden::halt_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    return fmt::println(hidden::halt_backend);
}

template <typename T>
auto print(T&& value) -> int {
    return fmt::print(hidden::halt_backend, std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println(hidden::halt_backend, std::forward<T>(value));
}

auto println(string format) -> int {
    return fmt::println(hidden::halt_backend, format);
}

template <typename T, typename... Rest>
auto println(string format, T&& value, Rest&&... rest) -> int {
    return fmt::println(hidden::halt_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

} // namespace halt

#endif
