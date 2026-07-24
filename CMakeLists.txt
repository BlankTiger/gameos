cmake_minimum_required(VERSION 3.20)
project(gameos C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)

# Graphics settings
set(GFX_WIDTH 1024)
set(GFX_HEIGHT 768)

# Configure configuration header
configure_file(${CMAKE_SOURCE_DIR}/src/config.hh.in ${GENERATED_DIR}/config.hh @ONLY)
# Configure GRUB
configure_file(${CMAKE_SOURCE_DIR}/grub.cfg.in ${GENERATED_DIR}/grub.cfg @ONLY)

# Build preprocessing tool
include(ExternalProject)

ExternalProject_Add(preprocessor_build
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tools
    BINARY_DIR ${CMAKE_BINARY_DIR}/tools
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/Toolchain-gcc.cmake
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(preprocessor_build BINARY_DIR)

set(PREPROCESSOR
    ${BINARY_DIR}/preprocessor
)

# Generate source files

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*
)

set(GENERATED_SOURCES
    ${GENERATED_DIR}/src/boot.S
    ${GENERATED_DIR}/src/main.cc
)

file(GLOB_RECURSE preprocessor_sources CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/preprocessor.cc
)

add_custom_command(
    OUTPUT
        ${GENERATED_DIR}/.stamp
        ${GENERATED_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}/src
    COMMAND ${PREPROCESSOR}
            ${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_SOURCE_DIR}/src
            ${GENERATED_DIR}/src
    COMMAND ${CMAKE_COMMAND} -E touch ${GENERATED_DIR}/.stamp
    DEPENDS preprocessor_build ${preprocessor_sources} ${source_files}
)

add_custom_target(generate_files
    DEPENDS ${GENERATED_DIR}/.stamp ${GENERATED_SOURCES}
)

add_executable(gameos
    ${GENERATED_SOURCES}
)

add_dependencies(gameos generate_files)

# Compiles every header standalone, so none of them can quietly depend
# on an include that some other header happened to pull in first.
file(GLOB_RECURSE all_headers CONFIGURE_DEPENDS ${GENERATED_DIR}/src/*.hh)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/header_checks)

foreach(header ${all_headers})
    file(RELATIVE_PATH header_rel ${CMAKE_BINARY_DIR} ${header})
    string(REPLACE "/" "_" header_flat ${header_rel})
    set(stamp ${CMAKE_BINARY_DIR}/header_checks/${header_flat}.stamp)
    add_custom_command(
        OUTPUT ${stamp}
        COMMAND ${CMAKE_CXX_COMPILER}
                -std=c++23 -ffreestanding -fno-exceptions -fno-rtti
                -Wno-pragma-once-outside-header
                -I ${GENERATED_DIR}/src -I ${GENERATED_DIR}
                -fsyntax-only ${header}
        COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
        DEPENDS ${header} ${GENERATED_DIR}/.stamp
    )
    list(APPEND header_stamps ${stamp})
endforeach()

add_custom_target(check_headers ALL DEPENDS ${header_stamps})

target_include_directories(gameos PRIVATE ${GENERATED_DIR})

target_compile_options(gameos PRIVATE
    $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C,CXX>:-O2>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -DGFX_WIDTH=${GFX_WIDTH}
    -DGFX_HEIGHT=${GFX_HEIGHT}
)

target_link_options(gameos PRIVATE
    -T ${GENERATED_DIR}/src/linker.ld
    -ffreestanding
    -nostdlib
)

target_link_libraries(gameos PRIVATE gcc)

add_custom_command(TARGET gameos POST_BUILD
    COMMAND grub-file --is-x86-multiboot2 $<TARGET_FILE:gameos>
    COMMAND_EXPAND_LISTS
)

add_custom_target(iso ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/isodir/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gameos> ${CMAKE_BINARY_DIR}/isodir/boot/gameos
    COMMAND ${CMAKE_COMMAND} -E copy ${GENERATED_DIR}/grub.cfg ${CMAKE_BINARY_DIR}/isodir/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/gameos.iso ${CMAKE_BINARY_DIR}/isodir
    DEPENDS gameos
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
