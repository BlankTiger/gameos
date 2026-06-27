# Development

    nix develop -c fish
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# Running

    qemu-system-i386 -cdrom build/gameos.iso

# Terminal interface (switched by using a different namespace)

Must implement all forward declared functions in `kstd/term.hh`. Management of
the current row, column, scrolling should be handled internally.

# TODO:

8-bit slowly drifting
