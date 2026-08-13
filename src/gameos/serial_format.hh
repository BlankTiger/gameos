#pragma once

#include "kstd/format.hh"

#include "gameos/serial_port.hh"

namespace serial {

auto print(string format) -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::print(serial_backend, format);
}

template <typename T, typename... Rest>
auto print(string format, T&& value, Rest&&... rest) -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::print(serial_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::println(serial_backend);
}

auto println(string format) -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::println(serial_backend, format);
}

template <typename T>
auto print(T&& value) -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::print(serial_backend, std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::println(serial_backend, std::forward<T>(value));
}

template <typename T, typename... Rest>
auto println(string format, T&& value, Rest&&... rest) -> int {
    using namespace hidden;
    auto scoped_lock = serial_backend.lock.scoped_lock();
    return fmt::println(serial_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

}
