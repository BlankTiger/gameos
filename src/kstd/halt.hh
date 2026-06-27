#pragma once

#include <source_location>

[[noreturn]] static auto halt_forever(
    const char* message,
    const std::source_location& location = std::source_location::current()) -> void {
    if (message != nullptr) {
        term::println("%", message);
    }

    term::println("%:%:%", location.file_name(), location.line(), location.column());

    for (;;) {
        asm volatile("hlt");
    }
}
