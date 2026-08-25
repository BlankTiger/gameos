cmake_minimum_required(VERSION 3.20)

project(preprocessor CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    add_link_options(-fuse-ld=mold)
endif()

find_package(Threads REQUIRED)

add_executable(preprocessor
    preprocessor.cc
)

target_compile_features(preprocessor PRIVATE cxx_std_23)
target_include_directories(preprocessor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_compile_options(preprocessor PRIVATE
    -Wall
    -Wextra
    -Wno-sign-compare
    -Wno-narrowing
    -pedantic
)

target_link_libraries(preprocessor PRIVATE Threads::Threads)
