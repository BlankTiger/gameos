# Development

    nix develop -c fish
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# VGA mode

Framebuffer mode is the default one, if you want VGA you can do:

    cmake -S . -B build -DUSE_VGA_BACKEND=ON -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# Running

    qemu-system-i386 -cdrom build/gameos.iso

# Terminal interface (switched by using a different namespace)

Must implement all forward declared functions in `kstd/term.hh`. Management of
the current row, column, scrolling should be handled internally.

# TODO:

8-bit slowly drifting
