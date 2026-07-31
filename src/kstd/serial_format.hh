#pragma once

#include "serial_port.hh"
#include "format.hh"

namespace serial {

auto print(string format) -> int {
    return fmt::print(hidden::serial_backend, format);
}

template <typename T, typename... Rest>
auto print(string format, T&& value, Rest&&... rest) -> int {
    return fmt::print(hidden::serial_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    return fmt::println(hidden::serial_backend);
}

auto println(string format) -> int {
    return fmt::println(hidden::serial_backend, format);
}

template <typename T>
auto print(T&& value) -> int {
    return fmt::print(hidden::serial_backend, std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println(hidden::serial_backend, std::forward<T>(value));
}

template <typename T, typename... Rest>
auto println(string format, T&& value, Rest&&... rest) -> int {
    return fmt::println(hidden::serial_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

}
