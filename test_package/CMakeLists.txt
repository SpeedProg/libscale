cmake_minimum_required(VERSION 3.20)
project(libscale_test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libscale REQUIRED)
# CONFIG mode: CMake's module-mode FindSQLite3 stops the search when it cannot
# find a system SQLite (the Windows case), which would block the Conan-provided
# configuration. This project only ever builds under a Conan toolchain, whose
# CMakeDeps generator always emits sqlite3-config.cmake.
find_package(SQLite3 CONFIG REQUIRED)
find_package(SqliteOrm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)
if(NOT WIN32)
    find_package(libusb REQUIRED)
endif()

add_executable(test_package src/example.cpp)
target_link_libraries(test_package PRIVATE
    libscale::libscale
    SQLite::SQLite3
    sqlite_orm::sqlite_orm
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)
