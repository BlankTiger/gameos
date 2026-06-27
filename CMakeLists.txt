cmake_minimum_required(VERSION 3.20)
project(gameos C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(gameos
    src/boot.S
    src/main.cc
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

option(USE_VGA_BACKEND "Use VGA text mode backend" OFF)
option(USE_FB_BACKEND "Use framebuffer backend" OFF)

if(USE_VGA_BACKEND AND USE_FB_BACKEND)
    message(FATAL_ERROR "USE_VGA_BACKEND and USE_FB_BACKEND are mutually exclusive. Specify only one.")
endif()

if(NOT USE_VGA_BACKEND AND NOT USE_FB_BACKEND)
    set(USE_FB_BACKEND ON)
endif()

if(USE_VGA_BACKEND)
    target_compile_definitions(gameos PRIVATE USE_VGA_BACKEND)
else()
    target_compile_definitions(gameos PRIVATE USE_FB_BACKEND)
endif()

if(USE_VGA_BACKEND)
    set(GRUB_GFX_SETTINGS "\tset gfxpayload=text")
else()
    set(GRUB_GFX_SETTINGS "\tterminal_output gfxterm\n\tset gfxmode=1024x768x32\n\tset gfxpayload=keep")
endif()

configure_file(${CMAKE_SOURCE_DIR}/grub.cfg.in ${CMAKE_BINARY_DIR}/grub.cfg @ONLY)

target_link_libraries(gameos PRIVATE gcc)

add_custom_command(TARGET gameos POST_BUILD
    COMMAND grub-file --is-x86-multiboot2 $<TARGET_FILE:gameos>
    COMMAND_EXPAND_LISTS
)

add_custom_target(iso ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/isodir/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gameos> ${CMAKE_BINARY_DIR}/isodir/boot/gameos
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/grub.cfg ${CMAKE_BINARY_DIR}/isodir/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/gameos.iso ${CMAKE_BINARY_DIR}/isodir
    DEPENDS gameos
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
