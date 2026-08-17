#pragma once

#include "kstd/format.hh"

#include "gameos/assert.hh"

#if !HOSTED

// halt::print needs fmt; assert.hh cannot include this header (cycle).
namespace halt {

auto print(string format) -> int {
    auto written = fmt::print(hidden::halt_backend, format);
    flush();
    return written;
}

template <typename T, typename... Rest>
auto print(string format, T&& value, Rest&&... rest) -> int {
    auto written = fmt::print(hidden::halt_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
    flush();
    return written;
}

auto println() -> int {
    auto written = fmt::println(hidden::halt_backend);
    flush();
    return written;
}

template <typename T>
auto print(T&& value) -> int {
    auto written = fmt::print(hidden::halt_backend, std::forward<T>(value));
    flush();
    return written;
}

template <typename T>
auto println(T&& value) -> int {
    auto written = fmt::println(hidden::halt_backend, std::forward<T>(value));
    flush();
    return written;
}

auto println(string format) -> int {
    auto written = fmt::println(hidden::halt_backend, format);
    flush();
    return written;
}

template <typename T, typename... Rest>
auto println(string format, T&& value, Rest&&... rest) -> int {
    auto written = fmt::println(hidden::halt_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
    flush();
    return written;
}

} // namespace halt

#endif
