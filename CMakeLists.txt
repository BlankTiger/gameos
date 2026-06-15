cmake_minimum_required(VERSION 3.20)
project(gameos C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(gameos
    src/boot.s
    src/kernel.cc
)

target_compile_options(gameos PRIVATE
    $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C,CXX>:-O2>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_link_options(gameos PRIVATE
    -T ${CMAKE_SOURCE_DIR}/src/linker.ld
    -ffreestanding
    -nostdlib
)

target_link_libraries(gameos PRIVATE gcc)

add_custom_command(TARGET gameos POST_BUILD
    COMMAND grub-file --is-x86-multiboot $<TARGET_FILE:gameos>
    COMMAND_EXPAND_LISTS
)

add_custom_target(iso ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/isodir/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gameos> ${CMAKE_BINARY_DIR}/isodir/boot/gameos
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub.cfg ${CMAKE_BINARY_DIR}/isodir/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/gameos.iso ${CMAKE_BINARY_DIR}/isodir
    DEPENDS gameos
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
