cmake_minimum_required(VERSION 3.20)

project(preprocessor CXX)

find_package(Threads REQUIRED)

add_executable(preprocessor
    preprocessing.cc
)

target_compile_features(preprocessor PRIVATE cxx_std_23)

target_compile_options(preprocessor PRIVATE
    -Wall
    -Wextra
    -pedantic
)

target_link_libraries(preprocessor PRIVATE Threads::Threads)
