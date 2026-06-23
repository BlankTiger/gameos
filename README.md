# Development

    nix develop -c fish
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# Running

    qemu-system-i386 -kernel build/gameos

# TODO:

8-bit slowly drifting
