#pragma once

// Forward declarations.
namespace vga {
namespace term {

// Initializes all state necessary for operating the interface correctly.
auto initialize() -> void;

// Prints a string.
auto print(const char* format) -> int;

// Prints any formatted string.
template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int;

// Moves cursor onto the next line.
auto println() -> int;

// Prints a string and moves cursor onto the next line.
auto println(const char* format) -> int;

// Prints any formatted string and moves cursor onto the next line.
template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int;

}  // namespace term
}  // namespace vga

namespace term = vga::term;
