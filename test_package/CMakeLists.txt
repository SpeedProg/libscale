cmake_minimum_required(VERSION 3.20)
project(libscale_test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libscale REQUIRED)

add_executable(test_package src/example.cpp)
target_link_libraries(test_package PRIVATE libscale::libscale)
