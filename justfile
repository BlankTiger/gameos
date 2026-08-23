build_dir      := "build"
test_build_dir := "build/tests"

# Auto-enter flake dev shell when missing (skip if already inside).
set shell := ["bash", "-c", 'if [ -z "${IN_NIX_SHELL:-}" ]; then exec nix develop --command bash -euo pipefail -c "$1"; else exec bash -euo pipefail -c "$1"; fi', "--"]

default:
    just --list

build: configure
    cmake --build {{build_dir}}

build-debug: configure-debug
    cmake --build {{build_dir}}

build-release: configure-release
    cmake --build {{build_dir}}

configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-elf.cmake

configure-debug:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-elf.cmake

configure-release:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-elf.cmake

preprocess: configure
    cmake --build {{build_dir}} --target generate_files

check-headers: preprocess
    cmake --build {{build_dir}} --target check_headers

# Compile only (no codegen/link).
syntax: preprocess
    cmake --build {{build_dir}} --target syntax_only

run: build
    qemu-system-x86_64 -enable-kvm -cpu host -m 512M -smp 4 -serial stdio -display sdl -cdrom {{build_dir}}/gameos.iso

run-debugger: build-debug
    qemu-system-x86_64 -enable-kvm -cpu host -m 512M -smp 4 -serial stdio -display sdl -no-reboot -no-shutdown -S -s -cdrom {{build_dir}}/gameos.iso

test: configure-tests
    cmake --build {{test_build_dir}}
    ctest --test-dir {{test_build_dir}} --output-on-failure

configure-tests:
    cmake -S tests -B {{test_build_dir}} -G Ninja -DCMAKE_CXX_COMPILER=g++

clean:
    rm -rf {{build_dir}}
