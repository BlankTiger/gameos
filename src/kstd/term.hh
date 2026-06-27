#pragma once

namespace mem {
struct Multiboot2_Info;
}

// Forward declarations.
#if defined(USE_FB_BACKEND)
namespace fb {
#elif defined(USE_VGA_BACKEND)
namespace vga {
#endif

namespace term {

// Initializes all state necessary for operating the interface correctly.
[[nodiscard]] auto initialize(const mem::Multiboot2_Info* mbi) -> bool;

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

}  // namespace vga/fb

#if defined(USE_VGA_BACKEND)
namespace term = vga::term;
#elif defined(USE_FB_BACKEND)
namespace term = fb::term;
#endif
