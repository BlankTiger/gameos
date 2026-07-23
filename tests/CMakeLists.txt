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
# Find those headers and generate a single driver .cc that defines the
# macro and includes all of them into ONE translation unit, so inline
# tests run through the same `kstd_tests` binary/ctest run as out-of-line
# ones. This must stay a single TU (not one driver per header): headers
# routinely include each other, and #pragma once only dedupes within a
# TU, so splitting into per-header TUs that get linked together would
# cause the same header's TEST() blocks to be compiled (and defined)
# more than once whenever one test-bearing header includes another.
set(GENERATED_INLINE_TEST_DIR ${CMAKE_BINARY_DIR}/generated_inline_tests)
file(MAKE_DIRECTORY ${GENERATED_INLINE_TEST_DIR})

file(GLOB_RECURSE all_headers CONFIGURE_DEPENDS ${SRC_DIR}/*.hh)

set(driver ${GENERATED_INLINE_TEST_DIR}/inline_tests.cc)
file(WRITE ${driver} "#define UNIT_TESTS\n#include <gtest/gtest.h>\n")

foreach(header ${all_headers})
    file(STRINGS ${header} has_inline_tests REGEX "UNIT_TESTS" LIMIT_COUNT 1)
    if(has_inline_tests)
        file(RELATIVE_PATH header_rel ${SRC_DIR} ${header})
        file(APPEND ${driver} "#include \"${header_rel}\"\n")
    endif()
endforeach()

set(inline_test_sources ${driver})

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
