cmake_minimum_required(VERSION 3.20)

project(kstd_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    add_link_options(-fuse-ld=mold)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

find_package(GTest REQUIRED)

enable_testing()
include(GoogleTest)

set(SRC_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../assets)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
set(GENERATED_SRC_DIR ${GENERATED_DIR}/src)

# src/ uses @embed/@T, so it must go through the same preprocessor as the
# main build before it's valid C++ that the tests can #include.
include(ExternalProject)

ExternalProject_Add(preprocessor_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../tools
    BINARY_DIR ${CMAKE_BINARY_DIR}/tools
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Toolchain-gcc.cmake
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    BUILD_ALWAYS  TRUE
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(preprocessor_build BINARY_DIR)

set(PREPROCESSOR ${BINARY_DIR}/preprocessor)

file(GLOB_RECURSE src_files CONFIGURE_DEPENDS ${SRC_DIR}/*)

add_custom_command(
    OUTPUT ${GENERATED_DIR}/.stamp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_SRC_DIR}
    COMMAND ${PREPROCESSOR} ${ASSETS_DIR} ${SRC_DIR} ${GENERATED_SRC_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${GENERATED_DIR}/.stamp
    DEPENDS preprocessor_build ${src_files}
)

add_custom_target(generate_test_files DEPENDS ${GENERATED_DIR}/.stamp)

# Headers embed TEST() blocks under per-file guards
# (`#ifdef UNIT_TESTS_<PATH>`, e.g. UNIT_TESTS_KSTD_ARRAY for
# kstd/array.hh) so they never leak into non-test builds.
# One driver .cc per such header: defines only that header's macro, so
# include-graph neighbors keep their tests off. Drivers are separate TUs
# in one binary so ninja compiles them in parallel (shared gtest PCH).
set(GENERATED_INLINE_TEST_DIR ${CMAKE_BINARY_DIR}/generated_inline_tests)
file(MAKE_DIRECTORY ${GENERATED_INLINE_TEST_DIR})

file(GLOB_RECURSE all_headers CONFIGURE_DEPENDS ${SRC_DIR}/*.hh)

set(inline_test_sources "")
foreach(header ${all_headers})
    file(STRINGS ${header} has_inline_tests REGEX "UNIT_TESTS_" LIMIT_COUNT 1)
    if(has_inline_tests)
        file(RELATIVE_PATH header_rel ${SRC_DIR} ${header})
        string(REPLACE "/" "_" macro_base "${header_rel}")
        string(REGEX REPLACE "\\.[^.]*$" "" macro_base "${macro_base}")
        string(TOUPPER "${macro_base}" macro_base)
        set(macro "UNIT_TESTS_${macro_base}")
        string(REPLACE "/" "_" driver_stem "${header_rel}")
        string(REGEX REPLACE "\\.[^.]*$" "" driver_stem "${driver_stem}")
        set(driver ${GENERATED_INLINE_TEST_DIR}/${driver_stem}_inline_tests.cc)
        file(WRITE ${driver}
            "#define ${macro}\n#include <gtest/gtest.h>\n#include \"${header_rel}\"\n")
        list(APPEND inline_test_sources ${driver})
    endif()
endforeach()

file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc
)

set(all_test_sources ${test_sources} ${inline_test_sources})
if(all_test_sources)
    add_executable(kstd_tests ${all_test_sources})
    add_dependencies(kstd_tests generate_test_files)
    target_include_directories(kstd_tests PRIVATE ${GENERATED_DIR} ${GENERATED_SRC_DIR})
    target_compile_definitions(kstd_tests PRIVATE UNIT_TEST)
    target_compile_options(kstd_tests PRIVATE
        -Wall
        -Wextra
        -Wno-sign-compare
        -Wno-narrowing
    )
    target_precompile_headers(kstd_tests PRIVATE
        <gtest/gtest.h>
    )
    target_link_libraries(kstd_tests PRIVATE GTest::gtest_main)
    gtest_discover_tests(kstd_tests)
    add_custom_target(build_tests DEPENDS kstd_tests)
endif()
