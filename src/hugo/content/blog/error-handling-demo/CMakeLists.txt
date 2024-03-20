cmake_minimum_required(VERSION 3.19..3.26)

project(error-handling-demo
    VERSION 0.1.0
    LANGUAGES CXX)

include(CTest)
enable_testing()

add_executable(error-handling-demo "main.cpp")
target_include_directories(error-handling-demo PUBLIC ".")
target_compile_features(error-handling-demo PUBLIC cxx_std_20)