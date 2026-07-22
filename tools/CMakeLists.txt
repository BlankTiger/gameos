cmake_minimum_required(VERSION 3.20)

project(preprocessor CXX)

add_executable(preprocessor
    preprocessing.cc
)

target_compile_features(preprocessor PRIVATE cxx_std_23)

target_compile_options(preprocessor PRIVATE
    -Wall
    -Wextra
    -pedantic
)
