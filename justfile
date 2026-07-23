build_dir      := "build"
test_build_dir := "build/tests"

default:
    just --list

build: configure
    cmake --build {{build_dir}}

configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake

preprocess: configure
    cmake --build {{build_dir}} --target generate_files

check-headers: preprocess
    cmake --build {{build_dir}} --target check_headers

run: build
    qemu-system-i386 -serial stdio -cdrom {{build_dir}}/gameos.iso

test: configure-tests
    cmake --build {{test_build_dir}}
    ctest --test-dir {{test_build_dir}} --output-on-failure

configure-tests:
    cmake -S tests -B {{test_build_dir}} -G Ninja -DCMAKE_CXX_COMPILER=g++

clean:
    rm -rf {{build_dir}}
