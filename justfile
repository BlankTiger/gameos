build_dir      := "build"
test_build_dir := "build/tests"

# Auto-enter flake dev shell when missing (skip if already inside).
set shell := ["bash", "-c", 'if [ -z "${IN_NIX_SHELL:-}" ]; then exec nix develop --command bash -euo pipefail -c "$1"; else exec bash -euo pipefail -c "$1"; fi', "--"]

default:
    just --list

build: configure
    cmake --build {{build_dir}}

configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-elf.cmake

preprocess: configure
    cmake --build {{build_dir}} --target generate_files

check-headers: preprocess
    cmake --build {{build_dir}} --target check_headers

run: build
    qemu-system-x86_64 -enable-kvm -cpu host -m 512M -serial stdio -cdrom {{build_dir}}/gameos.iso

test: configure-tests
    cmake --build {{test_build_dir}}
    ctest --test-dir {{test_build_dir}} --output-on-failure

configure-tests:
    cmake -S tests -B {{test_build_dir}} -G Ninja -DCMAKE_CXX_COMPILER=g++

clean:
    rm -rf {{build_dir}}
