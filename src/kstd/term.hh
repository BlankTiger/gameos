#pragma once

// Forward declarations.
namespace vga {
namespace term {
auto initialize() -> void;

auto print(const char* format) -> int;

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int;

auto println() -> int;

auto println(const char* format) -> int;

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int;
}  // namespace term
}  // namespace vga

namespace term = vga::term;
