# Development

    nix develop -c fish
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# Running

    qemu-system-i386 -cdrom build/gameos.iso

# Terminal interface (switched by using a different namespace)

Must contain:

    // Initializes all state necessary for operating the interface correctly.
    auto initialize() -> void;

    // Prints a string.
    auto print(const char* format) -> int;

    // Prints any formatted string.
    auto print(const char* format, T&& value, Rest&&.. rest) -> int;

    // Moves cursor onto the next line.
    auto println() -> int;

    // Prints a string and moves cursor onto the next line.
    auto println(const char* format) -> int;

    // Prints any formatted string and moves cursor onto the next line.
    auto println(const char* format, T&& value, Rest&&.. rest) -> int;

Management of the current row, column, scrolling should be handled internally.

# TODO:

8-bit slowly drifting
