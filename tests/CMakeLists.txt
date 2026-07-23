cmake_minimum_required(VERSION 3.20)

project(kstd_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

enable_testing()
include(GoogleTest)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc
)

# Some headers embed their own TEST() blocks inline, guarded by
# `#ifdef UNIT_TESTS` so they never leak into non-test builds.
# Find those headers and generate a one-line driver .cc per header
# that defines the macro and includes it, so inline tests run
# through the same `kstd_tests` binary/ctest run as out-of-line ones.
set(GENERATED_INLINE_TEST_DIR ${CMAKE_BINARY_DIR}/generated_inline_tests)
file(MAKE_DIRECTORY ${GENERATED_INLINE_TEST_DIR})

file(GLOB_RECURSE all_headers CONFIGURE_DEPENDS ${SRC_DIR}/*.hh)

set(inline_test_sources)
foreach(header ${all_headers})
    file(STRINGS ${header} has_inline_tests REGEX "UNIT_TESTS" LIMIT_COUNT 1)
    if(has_inline_tests)
        file(RELATIVE_PATH header_rel ${SRC_DIR} ${header})
        string(REPLACE "/" "_" header_flat ${header_rel})
        set(driver ${GENERATED_INLINE_TEST_DIR}/${header_flat}.cc)
        file(WRITE ${driver} "#define UNIT_TESTS\n#include <gtest/gtest.h>\n#include \"${header_rel}\"\n")
        list(APPEND inline_test_sources ${driver})
    endif()
endforeach()

add_executable(kstd_tests
    ${test_sources}
    ${inline_test_sources}
)

target_include_directories(kstd_tests PRIVATE ${SRC_DIR})

target_compile_options(kstd_tests PRIVATE
    -Wall
    -Wextra
)

target_link_libraries(kstd_tests PRIVATE GTest::gtest_main)

gtest_discover_tests(kstd_tests)
